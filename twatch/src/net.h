#pragma once

#include <stdint.h>

bool net_connect(const char *ssid, const char *pass, uint32_t timeoutMs);
void net_disconnect();

// Acerta o RTC por NTP. Só faz sentido com o WiFi já conectado.
// `tzMinutes` é o offset em relação ao UTC, das Settings.
bool net_sync_time(int16_t tzMinutes);
