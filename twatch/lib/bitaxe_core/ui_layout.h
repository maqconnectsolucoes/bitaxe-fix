#pragma once

#include <stdint.h>

#define SCREEN_W 240
#define SCREEN_H 240

// A faixa do botão de config ocupa os 48 px de baixo. Alvo grande de propósito:
// dedo em touch capacitivo de relógio erra muito mais que cursor de mouse.
#define CONFIG_STRIP_TOP_Y 192

enum ClockHit
{
    HIT_PAINEL,
    HIT_CONFIG,
};

ClockHit clock_hit(int16_t x, int16_t y);
