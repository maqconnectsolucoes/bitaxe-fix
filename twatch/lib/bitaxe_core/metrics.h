#pragma once

#include <stddef.h>
#include <stdint.h>

#include "bitaxe_status.h"

// Abaixo disto o J/TH não é exibido: a divisão estouraria e, com o minerador
// parado, o número não significaria nada.
#define METRICS_MIN_HASHRATE_GHS 1.0f

enum Freshness
{
    FRESHNESS_NOW,    // < 60 s: idade oculta
    FRESHNESS_AGING,  // 60 s a 10 min: idade visível
    FRESHNESS_STALE,  // >= 10 min: números acinzentados
};

bool metrics_efficiency(const BitaxeStatus &s, float &joulesPerTerahash);
Freshness metrics_freshness(uint32_t ageSeconds);
void metrics_format_hashrate(float ghs, char *buf, size_t cap);

// Separa valor e unidade porque a Font 6 do TFT_eSPI, a única grande o
// bastante para o número principal, não tem as letras de "Gh/s".
void metrics_format_hashrate_parts(float ghs, char *value, size_t vcap,
                                   char *unit, size_t ucap);
