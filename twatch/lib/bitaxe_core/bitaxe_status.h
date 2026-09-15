#pragma once

#include <stdint.h>

// Comprimento dos campos de texto. bestDiff chega como "1.2T"/"415M";
// hostname é o nome do aparelho na rede.
#define BITAXE_STR_LEN 24

// POD por exigência: esta struct é gravada em RTC_DATA_ATTR e precisa
// atravessar o deep sleep, quando o heap deixa de existir.
struct BitaxeStatus
{
    float hashRate10m;        // Gh/s, média de 10 min
    float power;              // W
    float frequency;          // MHz
    float coreVoltageActual;  // mV
    uint32_t sharesAccepted;
    char bestDiff[BITAXE_STR_LEN];
    char hostname[BITAXE_STR_LEN];
};
