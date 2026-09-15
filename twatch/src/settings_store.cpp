#include "settings_store.h"

#include <Preferences.h>

#include "config.h"

// Semente do segundo Bitaxe. Acrescentada na Fase 2: um config.h anterior a
// ela nao define BITAXE_HOST2, e sem esta guarda a compilacao quebraria para
// quem ainda nao atualizou o proprio config.h local.
#ifndef BITAXE_HOST2
#define BITAXE_HOST2 ""
#endif

// config.h e local e pode ser anterior a estes campos. Semente embutida em vez
// de erro de compilacao: quem nunca configurou brilho quer o padrao, nao um
// build quebrado.
#ifndef BRIGHTNESS_CLOCK
#define BRIGHTNESS_CLOCK 40
#endif
#ifndef BRIGHTNESS_PANEL
#define BRIGHTNESS_PANEL 100
#endif
#ifndef WATCH_INTERVAL_MIN
#define WATCH_INTERVAL_MIN 0
#endif

// Namespace do NVS. As chaves têm no máximo 15 caracteres — limite do NVS, e é
// por isso que são abreviadas.
static const char *NS = "twatch";

void settings_load(Settings &out)
{
    Preferences p;
    // Somente leitura. Se o namespace ainda não existe, begin() devolve false e
    // todo getX cai no default — exatamente o comportamento de primeiro boot.
    p.begin(NS, true);

    p.getString("ssid", WIFI_SSID).toCharArray(out.wifiSsid, sizeof(out.wifiSsid));
    p.getString("pass", WIFI_PASS).toCharArray(out.wifiPass, sizeof(out.wifiPass));
    p.getString("host", BITAXE_HOST).toCharArray(out.bitaxeHost, sizeof(out.bitaxeHost));
    p.getString("host2", BITAXE_HOST2).toCharArray(out.bitaxeHost2, sizeof(out.bitaxeHost2));

    out.tzMinutes = (int16_t) p.getInt("tz", TZ_MINUTES);
    out.wifiTimeoutMs = p.getUInt("wifims", WIFI_CONNECT_TIMEOUT_MS);
    out.httpTimeoutMs = p.getUInt("httpms", HTTP_TIMEOUT_MS);
    out.batteryMinPct = p.getUChar("battmin", BATTERY_MIN_PERCENT);
    out.idleRelogioMs = p.getUInt("idlerel", IDLE_RELOGIO_MS);
    out.idlePainelMs = p.getUInt("idlepan", IDLE_PAINEL_MS);
    out.brightnessClock = p.getUChar("brtclk", BRIGHTNESS_CLOCK);
    out.brightnessPanel = p.getUChar("brtpan", BRIGHTNESS_PANEL);
    out.watchIntervalMin = p.getUShort("watchmin", WATCH_INTERVAL_MIN);

    p.end();
}

bool settings_save(const Settings &s)
{
    Preferences p;
    if (!p.begin(NS, false)) {
        return false;
    }

    p.putString("ssid", s.wifiSsid);
    p.putString("pass", s.wifiPass);
    p.putString("host", s.bitaxeHost);
    p.putString("host2", s.bitaxeHost2);
    p.putInt("tz", s.tzMinutes);
    p.putUInt("wifims", s.wifiTimeoutMs);
    p.putUInt("httpms", s.httpTimeoutMs);
    p.putUChar("battmin", s.batteryMinPct);
    p.putUInt("idlerel", s.idleRelogioMs);
    p.putUInt("idlepan", s.idlePainelMs);
    p.putUChar("brtclk", s.brightnessClock);
    p.putUChar("brtpan", s.brightnessPanel);
    p.putUShort("watchmin", s.watchIntervalMin);
    p.end();

    // Confere relendo, em vez de somar os retornos de putX. Motivo concreto:
    // Preferences::putString devolve strlen(value), então gravar senha vazia
    // devolve 0 — indistinguível de erro. Reler resolve isso e ainda cobre
    // truncamento e chave que não coube.
    Settings check = {};
    settings_load(check);
    return settings_equal(check, s);
}
