#include "net.h"

#include <WiFi.h>
#include <time.h>

#include "config.h"

bool net_connect(const char *ssid, const char *pass, uint32_t timeoutMs)
{
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, pass);

    const uint32_t deadline = millis() + timeoutMs;
    while (WiFi.status() != WL_CONNECTED && millis() < deadline) {
        delay(100);
    }
    return WiFi.status() == WL_CONNECTED;
}

void net_disconnect()
{
    // O rádio é o maior consumidor: desligar antes de dormir é o ponto todo.
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
}

bool net_sync_time(int16_t tzMinutes)
{
    // Era configTime(0, ...), ou seja, UTC — o relógio mostrava três horas a
    // mais que Brasília. O offset agora vem das Settings.
    configTime((long) tzMinutes * 60, 0, NTP_SERVER);
    struct tm t;
    return getLocalTime(&t, 5000);
}
