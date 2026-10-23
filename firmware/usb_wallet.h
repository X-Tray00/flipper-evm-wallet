#pragma once
#include <furi.h>
#include <furi_hal_usb.h>
bool wallet_usb_start(FuriMessageQueue *packets);
void wallet_usb_stop(void);
bool wallet_usb_send(const uint8_t packet[64]);
bool wallet_usb_connected(void);
bool wallet_usb_fault(void);
