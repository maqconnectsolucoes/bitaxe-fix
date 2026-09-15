#pragma once

#include <LilyGoWatch.h>

// Aloca o sprite de 240x240 na PSRAM. Idempotente. false significa que o
// sprite nao coube: o desenho cai direto no display e volta a piscar, mas
// o firmware continua funcionando.
bool screen_begin(TTGOClass *watch);

// TFT_eSprite herda de TFT_eSPI e sobrescreve os primitivos como virtuais
// (TFT_eSPI.h:815), entao o mesmo codigo de desenho serve aos dois caminhos.
TFT_eSPI &screen_canvas(void);

// Empurra o sprite para o display. No-op quando nao ha sprite.
void screen_flush(void);

// Brilho em porcentagem. O fade evita o corte seco entre telas.
void screen_brightness(uint8_t percent, bool fade);
