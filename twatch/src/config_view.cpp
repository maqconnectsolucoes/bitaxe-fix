#include "config_view.h"

#include <stdio.h>

#include "screen.h"
#include "theme.h"

// O tick redesenha a tela inteira, entao precisa lembrar o que estava nela.
// Copias proprias: os ponteiros originais vem de constantes globais, mas
// depender disso seria uma armadilha para o proximo chamador.
static char g_ssid[33];
static char g_pass[33];
static char g_url[33];
static bool g_lowBattery = false;

static void drawAll(uint32_t secondsLeft)
{
    TFT_eSPI &c = screen_canvas();
    c.fillScreen(THEME_BG);
    c.setTextDatum(MC_DATUM);

    c.setFreeFont(FONT_LABEL);
    c.setTextColor(THEME_MUTED, THEME_BG);
    c.drawString("CONFIGURACAO", 120, 24);
    c.drawFastHLine(40, 40, 160, THEME_STALE);

    c.drawString("conecte o celular na rede", 120, 60);

    c.setFreeFont(FONT_VALUE);
    c.setTextColor(THEME_PRIMARY, THEME_BG);
    c.drawString(g_ssid, 120, 88);

    c.setFreeFont(FONT_LABEL);
    c.setTextColor(THEME_MUTED, THEME_BG);
    c.drawString("senha", 120, 116);
    c.setTextColor(THEME_PRIMARY, THEME_BG);
    c.drawString(g_pass, 120, 136);

    c.setTextColor(THEME_ACCENT, THEME_BG);
    c.drawString(g_url, 120, 160);

    if (g_lowBattery) {
        // Avisa, mas nao bloqueia: config e justamente o que se precisa quando
        // algo quebrou. O painel aborta com bateria baixa; aqui nao.
        c.setTextColor(THEME_WARN, THEME_BG);
        c.drawString("bateria baixa", 120, 182);
    }

    char buf[24];
    snprintf(buf, sizeof(buf), "fecha em %u:%02u", (unsigned) (secondsLeft / 60),
             (unsigned) (secondsLeft % 60));
    c.setTextColor(THEME_MUTED, THEME_BG);
    c.drawString(buf, 120, 206);
    c.drawString("toque p/ sair", 120, 228);

    screen_flush();
}

void config_view_draw(TTGOClass *watch, const char *apSsid, const char *apPass, const char *url,
                      uint32_t secondsLeft, bool lowBattery)
{
    (void) watch;

    snprintf(g_ssid, sizeof(g_ssid), "%s", apSsid);
    snprintf(g_pass, sizeof(g_pass), "%s", apPass);
    snprintf(g_url, sizeof(g_url), "%s", url);
    g_lowBattery = lowBattery;

    drawAll(secondsLeft);
}

void config_view_tick(TTGOClass *watch, uint32_t secondsLeft)
{
    (void) watch;
    drawAll(secondsLeft);
}

void config_view_message(TTGOClass *watch, const char *line1, const char *line2)
{
    (void) watch;

    TFT_eSPI &c = screen_canvas();
    c.fillScreen(THEME_BG);
    c.setTextDatum(MC_DATUM);

    c.setFreeFont(FONT_VALUE);
    c.setTextColor(THEME_PRIMARY, THEME_BG);
    c.drawString(line1, 120, 104);

    c.setFreeFont(FONT_LABEL);
    c.setTextColor(THEME_MUTED, THEME_BG);
    c.drawString(line2, 120, 138);

    screen_flush();
}
