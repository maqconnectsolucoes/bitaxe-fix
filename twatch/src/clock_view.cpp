#include "clock_view.h"

#include <math.h>
#include <stdio.h>

#include "battery.h"
#include "screen.h"
#include "theme.h"
#include "ui_layout.h"

// Engrenagem simples: disco com furo e seis dentes. As fontes do TFT_eSPI nao
// tem glifo de engrenagem, entao primitivas e o caminho.
static void drawGear(TFT_eSPI &c, int16_t cx, int16_t cy, uint16_t color)
{
    for (int i = 0; i < 6; i++) {
        const float a = (float) i * 3.14159265f / 3.0f;
        c.fillCircle(cx + (int16_t) (cosf(a) * 11.0f), cy + (int16_t) (sinf(a) * 11.0f), 3, color);
    }
    c.fillCircle(cx, cy, 9, color);
    c.fillCircle(cx, cy, 4, THEME_BG);
}

static uint16_t levelColor(BatteryLevel level)
{
    switch (level) {
    case BATTERY_LEVEL_LOW: return THEME_DANGER;
    case BATTERY_LEVEL_MID: return THEME_WARN;
    case BATTERY_LEVEL_HIGH: return THEME_GOOD;
    case BATTERY_LEVEL_UNKNOWN: break;
    }
    return THEME_MUTED;
}

// Pilha de 62x22 com terminal, seis segmentos internos, desenhada a partir do
// canto superior esquerdo.
static void drawBattery(TFT_eSPI &c, int16_t x, int16_t y, int percent, bool charging,
                        uint8_t minPct)
{
    const uint16_t color = levelColor(battery_level(percent, minPct));
    const int16_t bodyW = 62;
    const int16_t bodyH = 22;

    c.drawRoundRect(x, y, bodyW, bodyH, 4, color);
    c.fillRect(x + bodyW + 1, y + 7, 3, 8, color);

    const int filled = battery_segments(percent);
    for (int i = 0; i < filled; i++) {
        c.fillRect(x + 4 + i * 9, y + 4, 7, bodyH - 8, color);
    }

    if (charging) {
        // Raio sobre a pilha: dois triangulos, sem glifo disponivel na fonte.
        const int16_t cx = x + bodyW / 2;
        c.fillTriangle(cx + 3, y + 3, cx - 4, y + 12, cx, y + 12, THEME_PRIMARY);
        c.fillTriangle(cx, y + 10, cx + 4, y + 10, cx - 3, y + 19, THEME_PRIMARY);
    }

    char buf[8];
    if (percent == BATTERY_UNKNOWN) {
        snprintf(buf, sizeof(buf), "--%%");
    } else {
        snprintf(buf, sizeof(buf), "%d%%", percent);
    }

    c.setTextDatum(ML_DATUM);
    c.setFreeFont(FONT_LABEL);
    c.setTextColor(color, THEME_BG);
    c.drawString(buf, x + bodyW + 12, y + bodyH / 2);
}

void clock_view_draw(TTGOClass *watch, const ClockViewModel &vm)
{
    TFT_eSPI &c = screen_canvas();
    c.fillScreen(THEME_BG);

    // Bateria e engrenagem ANTES do ramo de hora invalida: aquele ramo termina
    // em return, e as duas coisas precisam existir justamente ali — e a tela em
    // que o usuario ainda nao configurou nada, e onde a carga mais importa.
    drawBattery(c, 62, 158, vm.batteryPercent, vm.charging, vm.batteryMinPct);

    c.drawFastHLine(40, CONFIG_STRIP_TOP_Y, 160, THEME_STALE);
    drawGear(c, 120, 214, THEME_MUTED);

    c.setTextDatum(MC_DATUM);

    if (!vm.timeIsValid) {
        // O PCF8563 nao descobre a hora sozinho, e a unica janela de rede e o
        // painel. Ate visita-lo uma vez, nao ha hora para mostrar.
        c.setFreeFont(FONT_HERO);
        c.setTextColor(THEME_STALE, THEME_BG);
        c.drawString("--:--", 120, 84);

        c.setFreeFont(FONT_LABEL);
        c.setTextColor(THEME_MUTED, THEME_BG);
        c.drawString("abra o painel", 120, 122);
        c.drawString("para acertar a hora", 120, 140);
        screen_flush();
        return;
    }

    RTC_Date now = watch->rtc->getDateTime();
    char buf[8];

    snprintf(buf, sizeof(buf), "%02d:%02d", now.hour, now.minute);
    c.setFreeFont(FONT_HERO);
    c.setTextColor(THEME_PRIMARY, THEME_BG);
    c.drawString(buf, 120, 84);

    // Data e passos dividem a linha: a data alinhada a esquerda da grade, os
    // passos a direita. Foi o unico espaco que sobrou — abaixo da pilha comeca
    // a faixa de toque do botao de config (CONFIG_STRIP_TOP_Y = 192), e acima
    // dela o caminho de hora invalida ja usa ate y=152.
    snprintf(buf, sizeof(buf), "%02d/%02d", now.day, now.month);
    c.setFreeFont(FONT_LABEL);
    c.setTextColor(THEME_MUTED, THEME_BG);
    c.setTextDatum(ML_DATUM);
    c.drawString(buf, GRID_LEFT, 126);

    char steps[24];
    snprintf(steps, sizeof(steps), "%lu passos", (unsigned long) vm.steps);
    c.setTextDatum(MR_DATUM);
    c.drawString(steps, GRID_RIGHT, 126);

    screen_flush();
}
