#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
void random_buffer(uint8_t *out, size_t n) { arc4random_buf(out, n); }
void tc_fault_handler(const char *msg) {
    (void)msg;
    abort();
}
