#pragma once

#include <LilyGoWatch.h>
#include <stdint.h>

// Struct em vez de parametros soltos: a Fase 3 acrescenta passos aqui sem
// mudar a assinatura de novo.
struct ClockViewModel
{
    bool timeIsValid;
    int batteryPercent;  // BATTERY_UNKNOWN quando nao ha leitura confiavel
    bool charging;
    uint8_t batteryMinPct;
    uint32_t steps;
};

void clock_view_draw(TTGOClass *watch, const ClockViewModel &vm);
