#pragma once

#include <LilyGoWatch.h>

// Paleta. Quatro papeis de texto e tres sinais — nada alem disso, e nenhuma
// cor literal TFT_* fora deste arquivo.
#define THEME_BG TFT_BLACK
#define THEME_PRIMARY TFT_WHITE
#define THEME_MUTED 0x8410    // cinza medio: rotulos e texto secundario
#define THEME_STALE 0x630C    // cinza escuro: dado velho, tela inteira
#define THEME_ACCENT 0xF483   // ambar #F7931A: o unico destaque

// Sinais. So aparecem na pilha de bateria e em notas de erro.
#define THEME_GOOD 0x3E4F    // verde suave
#define THEME_WARN 0xF647    // amarelo
#define THEME_DANGER 0xE228  // vermelho suave

// Tipografia. FreeSansBold24 tem ~34 px de caixa alta — menor que a Font7
// bitmap de 48 px que havia antes, e muito melhor desenhada.
#define FONT_HERO &FreeSansBold24pt7b
#define FONT_VALUE &FreeSansBold12pt7b
#define FONT_LABEL &FreeSans9pt7b

// Grade das linhas de rotulo/valor do painel: rotulo a esquerda, valor a direita.
#define GRID_LEFT 16
#define GRID_RIGHT 224
