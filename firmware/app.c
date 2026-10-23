#include "../core/protocol.h"
#include "../vendor/trezor-crypto/bip39.h"
#include "../vendor/trezor-crypto/memzero.h"
#include "platform.h"
#include "selftest.h"
#include "usb_wallet.h"
#include <bt/bt_service/bt.h>
#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <input/input.h>
#include <stdio.h>
#include <string.h>

typedef enum {
    Home,
    PinUnlock,
    PinNew,
    PinConfirm,
    Backup,
    WordEntry,
    Review,
    Notice,
    ReplaceWarning
} Screen;
typedef struct {
    FuriMutex *mutex;
    FuriMessageQueue *inputs;
    FuriMessageQueue *packets;
    ViewPort *viewport;
    Storage *storage;
    FwKeys keys;
    FwTransport transport;
    FwApproval approval;
    Screen screen;
    bool run, exists, recovering, quiz, changing, probe;
    volatile bool input_fault;
    uint8_t menu, page, account, digit, word_no, quiz_no, quiz_targets[3];
    uint32_t backup_seen, last_activity, pin_wait_until, failures, min_heap, selftest_ms;
    uint8_t selftest_result;
    char pin[13], pin_first[13], words[216], prefix[9], message[81];
    uint8_t pending_entropy[32];
    char letter;
} App;
static uint32_t now_ms(void) {
    return (uint32_t)((uint64_t)furi_get_tick() * 1000 / furi_kernel_get_tick_frequency());
}
static bool gpio(InputKey k) {
    const GpioPin *pins[] = {&gpio_button_up,   &gpio_button_down, &gpio_button_right,
                             &gpio_button_left, &gpio_button_ok,   &gpio_button_back};
    return k <= InputKeyBack && !furi_hal_gpio_read(pins[k]);
}
static void reply(App *a, uint8_t command, uint32_t session, uint32_t request, FwError error,
                  const uint8_t *data, size_t n) {
    (void)a;
    uint8_t response[81] = {0}, frame[64];
    if (n > 80)
        return;
    response[0] = error;
    if (n)
        memcpy(response + 1, data, n);
    for (size_t i = 0; i < (n + 1 + FW_FRAME_DATA - 1) / FW_FRAME_DATA; i++) {
        fw_frame_encode(frame, command | 0x80, session, request, n + 1, i, response);
        if (!wallet_usb_send(frame))
            break;
    }
    memzero(response, sizeof(response));
    memzero(frame, sizeof(frame));
}
static void cancel(App *a, FwError error) {
    if (a->approval.active)
        reply(a, a->transport.command, a->approval.session, a->approval.request, error, NULL, 0);
    fw_approval_cancel(&a->approval);
    fw_transport_cancel(&a->transport);
    if (a->screen == Review)
        a->screen = Home;
}
static void secrets_clear(App *a) {
    fw_lock(&a->keys);
    memzero(a->pin, sizeof(a->pin));
    memzero(a->pin_first, sizeof(a->pin_first));
    memzero(a->words, sizeof(a->words));
    memzero(a->pending_entropy, sizeof(a->pending_entropy));
    memzero(a->prefix, sizeof(a->prefix));
    mnemonic_clear();
    a->quiz = false;
    a->changing = false;
    a->recovering = false;
}
static void lock(App *a) {
    cancel(a, FW_ERR_LOCKED);
    secrets_clear(a);
    a->screen = Home;
    a->menu = 0;
}
static void notice(App *a, const char *s) {
    strlcpy(a->message, s, sizeof(a->message));
    a->screen = Notice;
}
static void working(App *a) {
    notice(a, "Working. Keep USB connected. PIN derivation may take some time.");
    furi_mutex_release(a->mutex);
    view_port_update(a->viewport);
    furi_delay_ms(35);
    furi_mutex_acquire(a->mutex, FuriWaitForever);
}
static void word_at(const App *a, uint8_t index, char out[9]) {
    const char *p = a->words;
    for (uint8_t i = 0; i < index && *p; i++) {
        while (*p && *p != ' ')
            p++;
        if (*p)
            p++;
    }
    size_t n = 0;
    while (p[n] && p[n] != ' ' && n < 8) {
        out[n] = p[n];
        n++;
    }
    out[n] = 0;
}
static void body(Canvas *c, const char *text) {
    /* Exact 20-character chunks. Nothing is ellipsized or omitted. */
    for (size_t line = 0; line < 4; line++) {
        size_t off = line * 20, n = strlen(text);
        if (off >= n)
            break;
        char s[21] = {0};
        size_t take = n - off;
        if (take > 20)
            take = 20;
        memcpy(s, text + off, take);
        canvas_draw_str(c, 2, 25 + line * 9, s);
    }
}
static void draw(Canvas *c, void *context) {
    App *a = context;
    if (furi_mutex_acquire(a->mutex, 20) != FuriStatusOk)
        return;
    canvas_clear(c);
    canvas_set_font(c, FontPrimary);
    char title[30] = "EVM Wallet", text[81] = "", footer[32] = "Back: cancel";
    switch (a->screen) {
    case Home:
        if (a->probe) {
            strcpy(text, "USB probe. No wallet keys loaded. Auto-exit in 30s.");
            strcpy(footer, "Test networks only");
        } else if (a->keys.unlocked) {
            const char *items[] = {"Show address", "Lock wallet", "Change PIN", "Exit"};
            snprintf(text, sizeof(text), "Unlocked: account %u", a->account + 1);
            snprintf(footer, sizeof(footer), "> %s", items[a->menu % 4]);
        } else {
            const char *items[] = {a->exists ? "Unlock" : "Create wallet", "Restore 24 words",
                                   "Exit"};
            strcpy(text, a->exists ? "Wallet locked" : "New test wallet. Never use real funds.");
            snprintf(footer, sizeof(footer), "> %s", items[a->menu % 3]);
        }
        break;
    case PinUnlock:
    case PinNew:
    case PinConfirm: {
        strlcpy(title,
                a->screen == PinUnlock ? "Unlock PIN"
                : a->screen == PinNew  ? "New PIN (8-12)"
                                       : "Repeat new PIN",
                sizeof(title));
        size_t n = strlen(a->pin);
        memset(text, '*', n);
        snprintf(text + n, sizeof(text) - n, "  [%u]", a->digit);
        strcpy(footer, "Up/Dn digit > add OK end");
        break;
    }
    case Backup: {
        char word[9];
        word_at(a, a->word_no, word);
        snprintf(title, sizeof(title), "Backup word %u / 24", a->word_no + 1);
        strlcpy(text, word, sizeof(text));
        memzero(word, sizeof(word));
        a->backup_seen |= 1u << a->word_no;
        strcpy(footer, "< > words   OK verify");
        break;
    }
    case WordEntry:
        snprintf(title, sizeof(title), a->quiz ? "Verify word %u" : "Restore word %u / 24",
                 a->quiz ? a->quiz_targets[a->quiz_no] + 1 : a->word_no + 1);
        snprintf(text, sizeof(text), "%s[%c]", a->prefix, a->letter);
        strcpy(footer, "Up/Dn letter > add OK end");
        break;
    case Review:
        if (a->page < a->approval.review.count) {
            FwPage *p = &a->approval.review.pages[a->page];
            strlcpy(title, p->title, sizeof(title));
            strlcpy(text, p->text, sizeof(text));
            uint64_t bit = (uint64_t)1 << a->page;
            if (!(a->approval.seen & bit))
                fw_approval_page(&a->approval, a->page);
            snprintf(footer, sizeof(footer), "%u/%u  < >   Hold OK 2s", a->page + 1,
                     a->approval.review.count);
        }
        break;
    case Notice:
        strcpy(title, "Wallet");
        strlcpy(text, a->message, sizeof(text));
        strcpy(footer, "OK / Back");
        break;
    case ReplaceWarning:
        strcpy(title, "Replace wallet?");
        strcpy(text, "Only proceed with your backup. A completed recovery replaces this wallet.");
        strcpy(footer, "OK proceed / Back cancel");
        break;
    }
    canvas_draw_str(c, 2, 11, title);
    canvas_set_font(c, FontSecondary);
    body(c, text);
    canvas_draw_str(c, 2, 63, footer);
    memzero(text, sizeof(text));
    furi_mutex_release(a->mutex);
}
static void input_callback(InputEvent *e, void *context) {
    App *a = context;
    if (furi_message_queue_put(a->inputs, e, 0) != FuriStatusOk)
        a->input_fault = true;
}
static void start_pin(App *a, Screen screen) {
    memzero(a->pin, sizeof(a->pin));
    a->digit = 0;
    a->screen = screen;
}
static void begin_backup(App *a) {
    const char *words = mnemonic_from_data(a->pending_entropy, 32);
    if (!words) {
        lock(a);
        notice(a, "Mnemonic generation failed");
        return;
    }
    strlcpy(a->words, words, sizeof(a->words));
    mnemonic_clear();
    a->word_no = 0;
    a->backup_seen = 0;
    a->screen = Backup;
}
static void begin_words(App *a) {
    a->screen = WordEntry;
    a->prefix[0] = 0;
    a->letter = 'a';
}
static void persist(App *a) {
    working(a);
    uint8_t record[128] = {0}, check[32] = {0};
    FwError e = fw_vault_seal(a->pending_entropy, a->pin_first, wallet_wrap, NULL, record);
    if (e == FW_OK)
        e = fw_vault_open(record, a->pin_first, wallet_wrap, NULL, check);
    if (e == FW_OK && !fw_equal(check, a->pending_entropy, 32))
        e = FW_ERR_CRYPTO;
    if (e == FW_OK)
        e = wallet_record_write(a->storage, record);
    memzero(record, sizeof(record));
    memzero(check, sizeof(check));
    if (e != FW_OK) {
        lock(a);
        notice(a, "Save incomplete. Keep backup. Reopen; try old/new PIN. Check SD card.");
        return;
    }
    memcpy(a->keys.entropy, a->pending_entropy, 32);
    a->keys.unlocked = true;
    memzero(a->pending_entropy, 32);
    memzero(a->pin_first, sizeof(a->pin_first));
    memzero(a->pin, sizeof(a->pin));
    memzero(a->words, sizeof(a->words));
    a->changing = false;
    a->recovering = false;
    a->quiz = false;
    a->exists = true;
    a->screen = Home;
    a->menu = 0;
    a->last_activity = now_ms();
}
static void pin_done(App *a) {
    if (!fw_pin_valid(a->pin))
        return;
    if (a->screen == PinUnlock) {
        uint32_t now = now_ms();
        if ((int32_t)(now - a->pin_wait_until) < 0) {
            memzero(a->pin, sizeof(a->pin));
            return;
        }
        working(a);
        uint8_t record[128] = {0};
        FwError e = wallet_record_read(a->storage, record);
        if (e == FW_OK)
            e = fw_vault_open(record, a->pin, wallet_wrap, NULL, a->keys.entropy);
        memzero(record, sizeof(record));
        memzero(a->pin, sizeof(a->pin));
        if (e == FW_OK) {
            a->keys.unlocked = true;
            a->failures = 0;
            a->screen = Home;
            a->menu = 0;
            a->last_activity = now_ms();
        } else {
            fw_lock(&a->keys);
            if (a->failures < 6)
                a->failures++;
            a->pin_wait_until = now_ms() + (1000u << a->failures);
            notice(a, "Unlock failed. Delay increases after failures. Check PIN and SD record.");
        }
    } else if (a->screen == PinNew) {
        strlcpy(a->pin_first, a->pin, sizeof(a->pin_first));
        start_pin(a, PinConfirm);
    } else if (!strcmp(a->pin, a->pin_first)) {
        memzero(a->pin, sizeof(a->pin));
        if (a->changing || a->recovering)
            persist(a);
        else
            begin_backup(a);
    } else {
        memzero(a->pin_first, sizeof(a->pin_first));
        start_pin(a, PinNew);
    }
}
static bool complete_word(const char *prefix, char word[9]) {
    size_t n = strlen(prefix);
    if (!n)
        return false;
    size_t matches = 0;
    for (size_t i = 0; i < BIP39_WORD_COUNT; i++)
        if (!strncmp(BIP39_WORDLIST_ENGLISH[i], prefix, n)) {
            strlcpy(word, BIP39_WORDLIST_ENGLISH[i], 9);
            matches++;
            if (!strcmp(BIP39_WORDLIST_ENGLISH[i], prefix))
                return true;
        }
    return matches == 1;
}
static void word_done(App *a) {
    char word[9] = {0};
    if (!complete_word(a->prefix, word))
        return;
    if (a->quiz) {
        char expected[9];
        word_at(a, a->quiz_targets[a->quiz_no], expected);
        bool ok = !strcmp(expected, word);
        memzero(expected, 9);
        memzero(word, 9);
        if (!ok) {
            a->quiz_no = 0;
            begin_words(a);
            return;
        }
        if (++a->quiz_no == 3) {
            persist(a);
            return;
        }
        begin_words(a);
    } else {
        size_t used = strlen(a->words);
        size_t n = strlen(word);
        if (used + n + 2 > sizeof(a->words)) {
            lock(a);
            return;
        }
        if (used)
            strcat(a->words, " ");
        strcat(a->words, word);
        memzero(word, 9);
        if (++a->word_no == 24) {
            FwError e = fw_mnemonic_restore(a->words, a->pending_entropy);
            memzero(a->words, sizeof(a->words));
            if (e == FW_OK)
                start_pin(a, PinNew);
            else {
                lock(a);
                notice(a, "Invalid 24-word checksum. No wallet was replaced.");
            }
        } else
            begin_words(a);
    }
}
static void show_address(App *a) {
    uint8_t address[20];
    if (fw_address(&a->keys, a->account, address) != FW_OK) {
        lock(a);
        return;
    }
    FwReview *r = malloc(sizeof(FwReview));
    memset(r, 0, sizeof(*r));
    r->kind = 1;
    r->account = a->account;
    char text[43] = "0x";
    fw_hex(address, 20, text + 2);
    fw_page(r, "Public ETH address", text);
    fw_page(r, "Account", "Test networks only. Verify entire address before receiving.");
    fw_approval_begin(&a->approval, r, 0, 0, now_ms());
    memzero(r, sizeof(*r));
    free(r);
    a->screen = Review;
    a->page = 0;
}
static void handle_input(App *a, const InputEvent *e) {
    if (e->sequence_source != INPUT_SEQUENCE_SOURCE_HARDWARE) {
        cancel(a, FW_ERR_CANCELLED);
        return;
    }
    if (e->type != InputTypePress || !gpio(e->key))
        return;
    a->last_activity = now_ms();
    if (a->probe) {
        if (e->key == InputKeyBack)
            a->run = false;
        return;
    }
    if (e->key == InputKeyBack) {
        if (a->screen == Home)
            a->run = false;
        else if (a->screen == Review)
            cancel(a, FW_ERR_CANCELLED);
        else if (a->screen == Notice)
            a->screen = Home;
        else
            lock(a);
        return;
    }
    if (a->screen == Notice) {
        if (e->key == InputKeyOk)
            a->screen = Home;
        return;
    }
    if (a->screen == ReplaceWarning) {
        if (e->key == InputKeyOk) {
            secrets_clear(a);
            a->recovering = true;
            a->word_no = 0;
            begin_words(a);
        }
        return;
    }
    if (a->screen == Home) {
        uint8_t count = a->keys.unlocked ? 4 : 3;
        if (e->key == InputKeyUp)
            a->menu = (a->menu + count - 1) % count;
        if (e->key == InputKeyDown)
            a->menu = (a->menu + 1) % count;
        if (e->key == InputKeyRight)
            a->account = (a->account + 1) % FW_ACCOUNTS;
        if (e->key == InputKeyLeft)
            a->account = (a->account + FW_ACCOUNTS - 1) % FW_ACCOUNTS;
        if (e->key != InputKeyOk)
            return;
        if (a->keys.unlocked) {
            switch (a->menu) {
            case 0:
                show_address(a);
                break;
            case 1:
                lock(a);
                break;
            case 2:
                memcpy(a->pending_entropy, a->keys.entropy, 32);
                a->changing = true;
                start_pin(a, PinNew);
                break;
            default:
                a->run = false;
                break;
            }
        } else if (a->menu == 0) {
            if (a->exists)
                start_pin(a, PinUnlock);
            else {
                furi_hal_random_fill_buf(a->pending_entropy, 32);
                a->recovering = false;
                start_pin(a, PinNew);
            }
        } else if (a->menu == 1)
            a->screen = ReplaceWarning;
        else
            a->run = false;
    } else if (a->screen == PinUnlock || a->screen == PinNew || a->screen == PinConfirm) {
        size_t n = strlen(a->pin);
        if (e->key == InputKeyUp)
            a->digit = (a->digit + 1) % 10;
        if (e->key == InputKeyDown)
            a->digit = (a->digit + 9) % 10;
        if (e->key == InputKeyRight && n < 12) {
            a->pin[n] = '0' + a->digit;
            a->pin[n + 1] = 0;
            a->digit = 0;
        }
        if (e->key == InputKeyLeft && n)
            a->pin[n - 1] = 0;
        if (e->key == InputKeyOk)
            pin_done(a);
    } else if (a->screen == Backup) {
        if (e->key == InputKeyRight && a->word_no < 23)
            a->word_no++;
        if (e->key == InputKeyLeft && a->word_no)
            a->word_no--;
        if (e->key == InputKeyOk && a->backup_seen == 0xffffff) {
            a->quiz = true;
            a->quiz_no = 0;
            for (size_t i = 0; i < 3; i++) {
                bool duplicate;
                do {
                    a->quiz_targets[i] = furi_hal_random_get() % 24;
                    duplicate = false;
                    for (size_t j = 0; j < i; j++)
                        if (a->quiz_targets[i] == a->quiz_targets[j])
                            duplicate = true;
                } while (duplicate);
            }
            begin_words(a);
        }
    } else if (a->screen == WordEntry) {
        size_t n = strlen(a->prefix);
        if (e->key == InputKeyUp)
            a->letter = a->letter == 'z' ? 'a' : a->letter + 1;
        if (e->key == InputKeyDown)
            a->letter = a->letter == 'a' ? 'z' : a->letter - 1;
        if (e->key == InputKeyRight && n < 8) {
            a->prefix[n] = a->letter;
            a->prefix[n + 1] = 0;
            a->letter = 'a';
        }
        if (e->key == InputKeyLeft && n)
            a->prefix[n - 1] = 0;
        if (e->key == InputKeyOk)
            word_done(a);
    } else if (a->screen == Review) {
        uint8_t old = a->page;
        if (e->key == InputKeyRight && a->page + 1 < a->approval.review.count)
            a->page++;
        if (e->key == InputKeyLeft && a->page)
            a->page--;
        if (old != a->page) {
            a->approval.released = false;
            a->approval.holding = false;
        }
    }
}
static bool add_origin(FwReview *r, const char *origin) {
    if (!fw_page(r, "Host origin",
                 "Reported by computer. Verify independently if computer is compromised."))
        return false;
    size_t n = strlen(origin);
    for (size_t i = 0; i < n; i += 80) {
        char part[81] = {0};
        size_t take = n - i;
        if (take > 80)
            take = 80;
        memcpy(part, origin + i, take);
        if (!fw_page(r, "Origin", part))
            return false;
    }
    return true;
}
static void request(App *a) {
    FwTransport *t = &a->transport;
    uint32_t id = t->request, s = t->session;
    uint8_t cmd = t->command;
    uint8_t extra[80] = {0};
    size_t n = 0;
    FwError error = FW_OK;
    if (cmd == 1) {
        cancel(a, FW_ERR_CANCELLED);
        s = furi_hal_random_get();
        if (!s)
            s = 1;
        fw_transport_reset(t, s);
        extra[0] = 1;
        extra[1] = 0;
        extra[2] = 0;
        fw_write32(extra + 3, a->min_heap);
        n = 7;
    } else if (cmd == 7) {
        if (t->size != 1 || t->data[0])
            error = FW_ERR_FORMAT;
        else {
            extra[0] = a->keys.unlocked;
            fw_write32(extra + 1, a->min_heap);
            n = 5;
            if (a->probe) {
                fw_write32(extra + 5, a->selftest_ms);
                extra[9] = a->selftest_result;
                n = 10;
            }
        }
    } else if (a->probe || !a->keys.unlocked || a->screen != Home)
        error = FW_ERR_LOCKED;
    else {
        /* Payload: account u8, origin length u8, ASCII origin, typed body. */
        if (t->size < 3 || t->data[0] >= FW_ACCOUNTS || !t->data[1] || t->data[1] > 127 ||
            (size_t)t->data[1] + 2 > t->size)
            error = FW_ERR_FORMAT;
        else {
            char origin[128] = {0};
            size_t on = t->data[1];
            memcpy(origin, t->data + 2, on);
            for (size_t i = 0; i < on; i++)
                if (origin[i] < 33 || origin[i] > 126)
                    error = FW_ERR_FORMAT;
            uint8_t account = t->data[0], address[20];
            const uint8_t *body = t->data + 2 + on;
            size_t len = t->size - 2 - on;
            FwReview *r = malloc(sizeof(FwReview));
            uint32_t heap = memmgr_get_free_heap();
            if (heap < a->min_heap)
                a->min_heap = heap;
            memset(r, 0, sizeof(*r));
            if (error == FW_OK)
                error = fw_address(&a->keys, account, address);
            if (error == FW_OK) {
                if (cmd == 2 && len == 0) {
                    r->kind = 1;
                    char text[43] = "0x";
                    fw_hex(address, 20, text + 2);
                    fw_page(r, "Share public address", text);
                } else if (cmd == 3)
                    error = fw_review_tx(body, len, address, r);
                else if (cmd == 4)
                    error =
                        fw_review_siwe(body, len, address, origin, furi_hal_rtc_get_timestamp(), r);
                else if (cmd == 5)
                    error = fw_review_permit(body, len, address, furi_hal_rtc_get_timestamp(), r);
                else
                    error = FW_ERR_FORMAT;
                if (error == FW_OK && !add_origin(r, origin))
                    error = FW_ERR_POLICY;
            }
            r->account = account;
            if (error == FW_OK && wallet_usb_connected() && !wallet_usb_fault()) {
                fw_approval_begin(&a->approval, r, s, id, now_ms());
                a->screen = Review;
                a->page = 0;
                a->last_activity = now_ms();
                memzero(r, sizeof(*r));
                free(r);
                return;
            }
            memzero(r, sizeof(*r));
            free(r);
            if (error == FW_OK) {
                error = FW_ERR_CANCELLED;
                lock(a);
            }
        }
    }
    reply(a, cmd, s, id, error, extra, n);
    fw_transport_cancel(t);
}
static void approved(App *a) {
    uint8_t out[65] = {0};
    FwError e;
    if (a->approval.session == 0) {
        fw_approval_cancel(&a->approval);
        a->screen = Home;
        return;
    }
    if (a->approval.review.kind == 1)
        e = fw_address(&a->keys, a->approval.review.account, out);
    else
        e = fw_sign(&a->keys, &a->approval.review, out);
    if (!wallet_usb_connected() || wallet_usb_fault() || furi_message_queue_get_count(a->packets) ||
        (uint32_t)(now_ms() - a->approval.started) >= FW_REVIEW_MS) {
        lock(a);
        fw_transport_reset(&a->transport, 0);
    } else {
        reply(a, a->transport.command, a->approval.session, a->approval.request, e, out,
              e == FW_OK ? (a->approval.review.kind == 1 ? 20 : 65) : 0);
        fw_approval_cancel(&a->approval);
        fw_transport_cancel(&a->transport);
        a->screen = Home;
    }
    memzero(out, sizeof(out));
    a->last_activity = now_ms();
}
int32_t wallet_main(void *args) {
    App *a = malloc(sizeof(App));
    memset(a, 0, sizeof(*a));
    a->run = true;
    a->probe = args && !strcmp((char *)args, "probe");
    a->letter = 'a';
    a->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    a->inputs = furi_message_queue_alloc(16, sizeof(InputEvent));
    a->packets = furi_message_queue_alloc(16, 64);
    a->storage = furi_record_open(RECORD_STORAGE);
    a->exists = wallet_record_exists(a->storage);
    a->viewport = view_port_alloc();
    view_port_draw_callback_set(a->viewport, draw, a);
    view_port_input_callback_set(a->viewport, input_callback, a);
    Gui *gui = furi_record_open(RECORD_GUI);
    Bt *bt = furi_record_open(RECORD_BT);
    bool bt_was = furi_hal_bt_is_active(), beacon_was = furi_hal_bt_extra_beacon_is_active();
    bt_disconnect(bt);
    furi_hal_bt_stop_advertising();
    if (beacon_was)
        furi_hal_bt_extra_beacon_stop();
    gui_add_view_port(gui, a->viewport, GuiLayerFullscreen);
    a->min_heap = memmgr_get_free_heap();
    if (a->probe) {
        uint32_t began = now_ms();
        a->selftest_result = wallet_selftest(&a->min_heap);
        a->selftest_ms = now_ms() - began;
    }
    bool usb = wallet_usb_start(a->packets);
    a->last_activity = now_ms();
    uint32_t started = a->last_activity;
    if (memmgr_get_free_heap() < a->min_heap)
        a->min_heap = memmgr_get_free_heap();
    if (!usb)
        notice(a, "USB setup failed. Back to exit.");
    while (a->run) {
        furi_mutex_acquire(a->mutex, FuriWaitForever);
        uint32_t now = now_ms();
        uint32_t heap = memmgr_get_free_heap();
        if (heap < a->min_heap)
            a->min_heap = heap;
        if (a->probe && (uint32_t)(now - started) >= 30000)
            a->run = false;
        if (a->input_fault || (usb && wallet_usb_fault())) {
            a->input_fault = false;
            lock(a);
            fw_transport_reset(&a->transport, 0);
        }
        if (furi_hal_bt_is_active() || furi_hal_bt_extra_beacon_is_active()) {
            lock(a);
            bt_disconnect(bt);
            furi_hal_bt_stop_advertising();
            furi_hal_bt_extra_beacon_stop();
        }
        InputEvent input;
        while (furi_message_queue_get(a->inputs, &input, 0) == FuriStatusOk)
            handle_input(a, &input);
        uint8_t packet[64];
        while (usb && furi_message_queue_get(a->packets, packet, 0) == FuriStatusOk) {
            if (a->approval.active) {
                cancel(a, FW_ERR_CANCELLED);
            }
            FwError e = fw_frame_receive(&a->transport, packet, now_ms());
            if (e == FW_OK && a->transport.complete)
                request(a);
            else if (e != FW_OK) {
                cancel(a, e);
                reply(a, packet[3] & 0x7f, fw_read32(packet + 4), fw_read32(packet + 8), e, NULL,
                      0);
            }
        }
        /* Key derivation and request parsing may take seconds. Sample again;
         * comparing an earlier tick with a new deadline wraps unsigned time. */
        now = now_ms();
        if (a->approval.active) {
            if ((uint32_t)(now - a->approval.started) >= FW_REVIEW_MS)
                cancel(a, FW_ERR_TIMEOUT);
            else if (fw_approval_button(&a->approval, true, gpio(InputKeyOk), now,
                                        a->approval.session ? a->transport.session : 0,
                                        a->approval.session ? a->transport.request : 0))
                approved(a);
        } else if ((uint32_t)(now - a->last_activity) >= FW_LOCK_MS) {
            lock(a);
            a->last_activity = now;
        }
        furi_mutex_release(a->mutex);
        view_port_update(a->viewport);
        furi_delay_ms(20);
    }
    furi_mutex_acquire(a->mutex, FuriWaitForever);
    lock(a);
    furi_mutex_release(a->mutex);
    if (usb)
        wallet_usb_stop();
    view_port_enabled_set(a->viewport, false);
    gui_remove_view_port(gui, a->viewport);
    view_port_free(a->viewport);
    if (bt_was)
        furi_hal_bt_start_advertising();
    if (beacon_was)
        furi_hal_bt_extra_beacon_start();
    furi_record_close(RECORD_BT);
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_STORAGE);
    furi_message_queue_free(a->inputs);
    furi_message_queue_free(a->packets);
    furi_mutex_free(a->mutex);
    memzero(a, sizeof(*a));
    free(a);
    return 0;
}
