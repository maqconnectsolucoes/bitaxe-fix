#pragma once

#include <stdint.h>

#define WATCHDOG_MAX_DEVICES 2

struct WatchdogInputs
{
    uint8_t deviceCount;
    bool online[WATCHDOG_MAX_DEVICES];      // respondeu nesta rodada
    float hashRate[WATCHDOG_MAX_DEVICES];   // Gh/s; irrelevante quando offline
};

// Verdadeiro quando algum dispositivo configurado nao respondeu ou parou de
// minerar. E o gatilho da vibracao — silencio significa "esta tudo bem".
bool watchdog_should_alert(const WatchdogInputs &in);

// Verdadeiro quando a vigia periodica deve ser armada antes de dormir.
bool watchdog_should_run(uint32_t intervalMinutes, int batteryPercent, uint8_t minPct);
