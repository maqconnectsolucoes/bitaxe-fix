#include "panel_view.h"

#include <stdio.h>

#include "metrics.h"
#include "screen.h"
#include "theme.h"

// Uma linha da grade: rotulo em maiusculas a esquerda, valor a direita.
static void drawRow(TFT_eSPI &c, int16_t y, const char *label, const char *value,
                    uint16_t valueColor)
{
    c.setTextDatum(TL_DATUM);
    c.setFreeFont(FONT_LABEL);
    c.setTextColor(THEME_MUTED, THEME_BG);
    c.drawString(label, GRID_LEFT, y);

    // TFT_eSPI soma glyph_ab (ascendente da fonte) ao y em fontes free, entao
    // com datum TL/TR "y" vira o topo da caixa-alta. O -4 e a diferenca de
    // ascendente entre FONT_VALUE e FONT_LABEL, para as duas ficarem na mesma linha de base.
    c.setTextDatum(TR_DATUM);
    c.setFreeFont(FONT_VALUE);
    c.setTextColor(valueColor, THEME_BG);
    c.drawString(value, GRID_RIGHT, y - 4);
}

void panel_view_draw(TTGOClass *watch, const BitaxeStatus *status, uint32_t ageSeconds,
                     const char *note, int deviceIndex, uint8_t deviceCount)
{
    (void) watch;

    TFT_eSPI &c = screen_canvas();
    c.fillScreen(THEME_BG);

    if (!status) {
        c.setTextDatum(MC_DATUM);
        c.setFreeFont(FONT_VALUE);
        c.setTextColor(THEME_MUTED, THEME_BG);
        c.drawString("sem dados ainda", 120, 108);
        if (note) {
            c.setFreeFont(FONT_LABEL);
            c.setTextColor(THEME_WARN, THEME_BG);
            c.drawString(note, 120, 140);
        }
        screen_flush();
        return;
    }

    // Acima de 10 min os numeros continuam legiveis, mas param de parecer atuais.
    const Freshness fresh = metrics_freshness(ageSeconds);
    const bool stale = fresh == FRESHNESS_STALE;
    const uint16_t primary = stale ? THEME_STALE : THEME_PRIMARY;
    const uint16_t accent = stale ? THEME_STALE : THEME_ACCENT;
    const uint16_t muted = stale ? THEME_STALE : THEME_MUTED;

    char buf[40];

    // Cabecalho: hostname a esquerda, idade a direita.
    c.setTextDatum(TL_DATUM);
    c.setFreeFont(FONT_LABEL);
    c.setTextColor(muted, THEME_BG);
    c.drawString(status->hostname[0] ? status->hostname : "bitaxe", GRID_LEFT, 12);

    if (fresh != FRESHNESS_NOW) {
        snprintf(buf, sizeof(buf), "ha %lu min", (unsigned long) (ageSeconds / 60));
        c.setTextDatum(TR_DATUM);
        c.drawString(buf, GRID_RIGHT, 12);
    }

    // Indicador de dispositivo: so existe quando ha dois. Fica no topo centro,
    // entre o hostname e a idade.
    if (deviceCount > 1) {
        for (uint8_t i = 0; i < deviceCount; i++) {
            const int16_t cx = 120 - ((int16_t) (deviceCount - 1) * 6) + (int16_t) i * 12;
            if ((int) i == deviceIndex) {
                c.fillCircle(cx, 17, 3, accent);
            } else {
                c.drawCircle(cx, 17, 3, muted);
            }
        }
    }

    // Numero heroi: so o valor, alinhado a esquerda na mesma grade do resto.
    char value[24];
    char unit[8];
    metrics_format_hashrate_parts(status->hashRate10m, value, sizeof(value), unit, sizeof(unit));

    c.setTextDatum(TL_DATUM);
    c.setFreeFont(FONT_HERO);
    c.setTextColor(accent, THEME_BG);
    c.drawString(value, GRID_LEFT, 36);

    // Unidade e eficiencia dividem a linha de baixo, em texto secundario.
    float jth = 0.0f;
    if (metrics_efficiency(*status, jth)) {
        snprintf(buf, sizeof(buf), "%s - %.1f J/TH", unit, jth);
    } else {
        snprintf(buf, sizeof(buf), "%s - -- J/TH", unit);
    }
    c.setFreeFont(FONT_LABEL);
    c.setTextColor(muted, THEME_BG);
    c.drawString(buf, GRID_LEFT, 88);

    snprintf(buf, sizeof(buf), "%.1f W", status->power);
    drawRow(c, 124, "POTENCIA", buf, primary);

    snprintf(buf, sizeof(buf), "%.0f MHz", status->frequency);
    drawRow(c, 150, "CLOCK", buf, primary);

    snprintf(buf, sizeof(buf), "%.0f mV", status->coreVoltageActual);
    drawRow(c, 176, "CORE", buf, primary);

    snprintf(buf, sizeof(buf), "%s - %lu ok", status->bestDiff[0] ? status->bestDiff : "-",
             (unsigned long) status->sharesAccepted);
    drawRow(c, 202, "BEST", buf, stale ? THEME_STALE : THEME_ACCENT);

    if (note) {
        c.setTextDatum(BC_DATUM);
        c.setFreeFont(FONT_LABEL);
        c.setTextColor(THEME_WARN, THEME_BG);
        c.drawString(note, 120, 236);
    }

    screen_flush();
}
