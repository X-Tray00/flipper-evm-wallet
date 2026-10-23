/* Custom vendor HID, using the public libusb_stm32/Furi HAL interfaces. */
#include "usb_wallet.h"
#include <string.h>
#include <usb_hid.h>
#define EP_TX 0x81
#define EP_RX 0x01
static usbd_device *device;
static FuriMessageQueue *rx;
static FuriSemaphore *tx_ready;
static volatile bool connected, fault;
static FuriHalUsbInterface *previous;
static bool was_locked;
static const uint8_t report[] = {0x06, 0x71, 0xff, 0x09, 0x01, 0xa1, 0x01, 0x15, 0x00,
                                 0x26, 0xff, 0x00, 0x75, 0x08, 0x95, 0x40, 0x09, 0x02,
                                 0x81, 0x02, 0x95, 0x40, 0x09, 0x03, 0x91, 0x02, 0xc0};
static const struct usb_string_descriptor maker = USB_STRING_DESC("Flipper EVM Wallet");
static const struct usb_string_descriptor name = USB_STRING_DESC("EVM Wallet Testnet");
static struct usb_device_descriptor desc = {.bLength = sizeof(struct usb_device_descriptor),
                                            .bDescriptorType = USB_DTYPE_DEVICE,
                                            .bcdUSB = 0x0200,
                                            .bDeviceClass = 0,
                                            .bMaxPacketSize0 = 8,
                                            .idVendor = 0x0483,
                                            .idProduct = 0x5741,
                                            .bcdDevice = 0x0001,
                                            .iManufacturer = 1,
                                            .iProduct = 2,
                                            .bNumConfigurations = 1};
static const struct __attribute__((packed)) {
    struct usb_config_descriptor cfg;
    struct usb_interface_descriptor intf;
    struct usb_hid_descriptor hid;
    struct usb_endpoint_descriptor input, output;
} config = {.cfg = {.bLength = sizeof(struct usb_config_descriptor),
                    .bDescriptorType = USB_DTYPE_CONFIGURATION,
                    .wTotalLength = sizeof(config),
                    .bNumInterfaces = 1,
                    .bConfigurationValue = 1,
                    .bmAttributes = 0x80,
                    .bMaxPower = 50},
            .intf = {.bLength = sizeof(struct usb_interface_descriptor),
                     .bDescriptorType = USB_DTYPE_INTERFACE,
                     .bNumEndpoints = 2,
                     .bInterfaceClass = USB_CLASS_HID},
            .hid = {.bLength = sizeof(struct usb_hid_descriptor),
                    .bDescriptorType = USB_DTYPE_HID,
                    .bcdHID = 0x0111,
                    .bNumDescriptors = 1,
                    .bDescriptorType0 = USB_DTYPE_HID_REPORT,
                    .wDescriptorLength0 = sizeof(report)},
            .input = {.bLength = sizeof(struct usb_endpoint_descriptor),
                      .bDescriptorType = USB_DTYPE_ENDPOINT,
                      .bEndpointAddress = EP_TX,
                      .bmAttributes = USB_EPTYPE_INTERRUPT,
                      .wMaxPacketSize = 64,
                      .bInterval = 5},
            .output = {.bLength = sizeof(struct usb_endpoint_descriptor),
                       .bDescriptorType = USB_DTYPE_ENDPOINT,
                       .bEndpointAddress = EP_RX,
                       .bmAttributes = USB_EPTYPE_INTERRUPT,
                       .wMaxPacketSize = 64,
                       .bInterval = 5}};
static void endpoint(usbd_device *d, uint8_t event, uint8_t ep) {
    (void)ep;
    if (event == usbd_evt_eptx) {
        furi_semaphore_release(tx_ready);
        return;
    }
    uint8_t packet[64];
    int32_t n = usbd_ep_read(d, EP_RX, packet, 64);
    if (n != 64 || furi_message_queue_put(rx, packet, 0) != FuriStatusOk)
        fault = true;
}
static usbd_respond configure(usbd_device *d, uint8_t cfg) {
    if (cfg == 0) {
        connected = false;
        fault = true;
        usbd_ep_deconfig(d, EP_TX);
        usbd_ep_deconfig(d, EP_RX);
        usbd_reg_endpoint(d, EP_TX, NULL);
        usbd_reg_endpoint(d, EP_RX, NULL);
        return usbd_ack;
    }
    if (cfg != 1)
        return usbd_fail;
    usbd_ep_config(d, EP_TX, USB_EPTYPE_INTERRUPT, 64);
    usbd_ep_config(d, EP_RX, USB_EPTYPE_INTERRUPT, 64);
    usbd_reg_endpoint(d, EP_TX, endpoint);
    usbd_reg_endpoint(d, EP_RX, endpoint);
    connected = true;
    return usbd_ack;
}
static usbd_respond control(usbd_device *d, usbd_ctlreq *req, usbd_rqc_callback *cb) {
    (void)cb;
    if (req->wIndex != 0)
        return usbd_fail;
    if (req->bmRequestType == 0x21 && req->bRequest == USB_HID_SETIDLE)
        return usbd_ack;
    if (req->bmRequestType != 0x81 || req->bRequest != USB_STD_GET_DESCRIPTOR)
        return usbd_fail;
    uint8_t type = req->wValue >> 8;
    if (type == USB_DTYPE_HID_REPORT) {
        d->status.data_ptr = (uint8_t *)report;
        d->status.data_count = sizeof(report);
        return usbd_ack;
    }
    if (type == USB_DTYPE_HID) {
        d->status.data_ptr = (uint8_t *)&config.hid;
        d->status.data_count = sizeof(config.hid);
        return usbd_ack;
    }
    return usbd_fail;
}
static void init(usbd_device *d, FuriHalUsbInterface *intf, void *ctx) {
    (void)intf;
    (void)ctx;
    device = d;
    usbd_reg_config(d, configure);
    usbd_reg_control(d, control);
    usbd_connect(d, true);
}
static void deinit(usbd_device *d) {
    connected = false;
    fault = true;
    configure(d, 0);
    usbd_reg_config(d, NULL);
    usbd_reg_control(d, NULL);
}
static void wake(usbd_device *d) {
    (void)d;
    connected = true;
}
static void suspend(usbd_device *d) {
    (void)d;
    connected = false;
    fault = true;
}
static FuriHalUsbInterface interface = {.init = init,
                                        .deinit = deinit,
                                        .wakeup = wake,
                                        .suspend = suspend,
                                        .dev_descr = &desc,
                                        .str_manuf_descr = (void *)&maker,
                                        .str_prod_descr = (void *)&name,
                                        .cfg_descr = (void *)&config};
bool wallet_usb_start(FuriMessageQueue *queue) {
    rx = queue;
    fault = false;
    connected = false;
    tx_ready = furi_semaphore_alloc(1, 1);
    previous = furi_hal_usb_get_config();
    was_locked = furi_hal_usb_is_locked();
    furi_hal_usb_unlock();
    if (!furi_hal_usb_set_config(&interface, NULL)) {
        furi_semaphore_free(tx_ready);
        tx_ready = NULL;
        if (was_locked)
            furi_hal_usb_lock();
        return false;
    }
    furi_hal_usb_lock();
    return true;
}
void wallet_usb_stop(void) {
    furi_hal_usb_unlock();
    furi_hal_usb_set_config(previous ? previous : &usb_cdc_single, NULL);
    if (was_locked)
        furi_hal_usb_lock();
    furi_semaphore_free(tx_ready);
    tx_ready = NULL;
    rx = NULL;
    device = NULL;
}
bool wallet_usb_connected(void) { return connected; }
bool wallet_usb_fault(void) {
    bool f = fault;
    fault = false;
    return f;
}
bool wallet_usb_send(const uint8_t p[64]) {
    if (!connected || furi_semaphore_acquire(tx_ready, 200) != FuriStatusOk)
        return false;
    if (!connected || usbd_ep_write(device, EP_TX, p, 64) != 64) {
        furi_semaphore_release(tx_ready);
        return false;
    }
    return true;
}
