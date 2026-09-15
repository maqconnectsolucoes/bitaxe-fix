#include "settings.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

// Parser estrito. strtol sozinho aceita "12abc" e devolve 12 — não serve para
// validar formulário, onde lixo digitado tem que reprovar, não ser ignorado.
static bool parse_i32(const char *s, int32_t &out)
{
    if (s == NULL || *s == '\0') {
        return false;
    }
    errno = 0;
    char *end = NULL;
    const long v = strtol(s, &end, 10);
    if (end == s || *end != '\0' || errno == ERANGE) {
        return false;
    }
    out = (int32_t) v;
    return true;
}

// Confere a faixa ANTES de estreitar para int16_t/uint8_t: converter primeiro
// truncaria 100000 em algo que passaria na validação seguinte.
static bool parse_range(const char *s, int32_t lo, int32_t hi, int32_t &out)
{
    int32_t v = 0;
    if (!parse_i32(s, v) || v < lo || v > hi) {
        return false;
    }
    out = v;
    return true;
}

// Recusa em vez de truncar: um SSID cortado no meio conecta em lugar nenhum e
// o usuário não teria como saber por quê.
static bool copy_string(char *dst, size_t cap, const char *src)
{
    if (src == NULL) {
        return false;
    }
    const size_t n = strlen(src);
    if (n + 1 > cap) {
        return false;
    }
    memcpy(dst, src, n + 1);
    return true;
}

// IPv4 estrito: quatro octetos de 0 a 255, no máximo três dígitos cada, e nada
// sobrando no fim. Hostname reprova de propósito.
static bool valid_ipv4(const char *s)
{
    for (int octet = 0; octet < 4; octet++) {
        if (*s < '0' || *s > '9') {
            return false;
        }
        int value = 0;
        int digits = 0;
        while (*s >= '0' && *s <= '9') {
            value = value * 10 + (*s - '0');
            digits++;
            if (digits > 3 || value > 255) {
                return false;
            }
            s++;
        }
        if (octet < 3) {
            if (*s != '.') {
                return false;
            }
            s++;
        }
    }
    return *s == '\0';
}

SettingsError settings_validate(const Settings &s)
{
    const size_t ssidLen = strlen(s.wifiSsid);
    if (ssidLen < 1 || ssidLen > SETTINGS_SSID_MAX) {
        return SETTINGS_ERR_SSID;
    }

    // Vazia é legítima: rede aberta. O que não existe é senha de 1 a 7.
    const size_t passLen = strlen(s.wifiPass);
    if (passLen != 0 && (passLen < SETTINGS_PASS_MIN || passLen > SETTINGS_PASS_MAX)) {
        return SETTINGS_ERR_PASS;
    }

    if (!valid_ipv4(s.bitaxeHost)) {
        return SETTINGS_ERR_HOST;
    }

    // So valida quando preenchido. Recusar igual ao primeiro pega o erro de
    // digitacao que produziria duas telas identicas sem sintoma nenhum.
    if (s.bitaxeHost2[0] != '\0') {
        if (!valid_ipv4(s.bitaxeHost2) || strcmp(s.bitaxeHost2, s.bitaxeHost) == 0) {
            return SETTINGS_ERR_HOST2;
        }
    }

    if (s.tzMinutes < SETTINGS_TZ_MIN_MINUTES || s.tzMinutes > SETTINGS_TZ_MAX_MINUTES ||
        s.tzMinutes % SETTINGS_TZ_STEP_MINUTES != 0) {
        return SETTINGS_ERR_TZ;
    }

    if (s.wifiTimeoutMs < SETTINGS_WIFI_TIMEOUT_MIN_MS || s.wifiTimeoutMs > SETTINGS_WIFI_TIMEOUT_MAX_MS) {
        return SETTINGS_ERR_WIFI_TIMEOUT;
    }
    if (s.httpTimeoutMs < SETTINGS_HTTP_TIMEOUT_MIN_MS || s.httpTimeoutMs > SETTINGS_HTTP_TIMEOUT_MAX_MS) {
        return SETTINGS_ERR_HTTP_TIMEOUT;
    }
    if (s.batteryMinPct > SETTINGS_BATTERY_MAX_PCT) {
        return SETTINGS_ERR_BATTERY;
    }
    if (s.idleRelogioMs < SETTINGS_IDLE_RELOGIO_MIN_MS || s.idleRelogioMs > SETTINGS_IDLE_RELOGIO_MAX_MS) {
        return SETTINGS_ERR_IDLE_RELOGIO;
    }
    if (s.idlePainelMs < SETTINGS_IDLE_PAINEL_MIN_MS || s.idlePainelMs > SETTINGS_IDLE_PAINEL_MAX_MS) {
        return SETTINGS_ERR_IDLE_PAINEL;
    }

    if (s.brightnessClock < SETTINGS_BRIGHTNESS_MIN_PCT ||
        s.brightnessClock > SETTINGS_BRIGHTNESS_MAX_PCT ||
        s.brightnessPanel < SETTINGS_BRIGHTNESS_MIN_PCT ||
        s.brightnessPanel > SETTINGS_BRIGHTNESS_MAX_PCT) {
        return SETTINGS_ERR_BRIGHTNESS;
    }

    // Zero e valido e significa desligada; qualquer outro valor tem que caber
    // na faixa util.
    if (s.watchIntervalMin != 0 && (s.watchIntervalMin < SETTINGS_WATCH_MIN_MINUTES ||
                                    s.watchIntervalMin > SETTINGS_WATCH_MAX_MINUTES)) {
        return SETTINGS_ERR_WATCH;
    }

    return SETTINGS_OK;
}

const char *settings_error_message(SettingsError e)
{
    switch (e) {
    case SETTINGS_OK: return "";
    case SETTINGS_ERR_SSID: return "Nome da rede: 1 a 32 caracteres.";
    case SETTINGS_ERR_PASS: return "Senha: deixe em branco (rede aberta) ou use de 8 a 63 caracteres.";
    case SETTINGS_ERR_HOST: return "IP do Bitaxe: use o IPv4 do aparelho, por exemplo 192.168.1.100.";
    case SETTINGS_ERR_HOST2: return "IP do segundo Bitaxe: use um IPv4 diferente do primeiro, ou deixe em branco.";
    case SETTINGS_ERR_TZ: return "Fuso invalido.";
    case SETTINGS_ERR_WIFI_TIMEOUT: return "Timeout de WiFi: 1000 a 30000 ms.";
    case SETTINGS_ERR_HTTP_TIMEOUT: return "Timeout de HTTP: 1000 a 15000 ms.";
    case SETTINGS_ERR_BATTERY: return "Bateria minima: 0 a 50 por cento.";
    case SETTINGS_ERR_IDLE_RELOGIO: return "Ociosidade do relogio: 2000 a 60000 ms.";
    case SETTINGS_ERR_IDLE_PAINEL: return "Ociosidade do painel: 5000 a 120000 ms.";
    case SETTINGS_ERR_BRIGHTNESS: return "Brilho: 10 a 100 por cento.";
    case SETTINGS_ERR_WATCH: return "Vigia: 0 para desligar, ou de 5 a 240 minutos.";
    }
    return "Configuracao invalida.";
}

SettingsError settings_apply_form(const Settings &current, const SettingsForm &f, Settings &out)
{
    out = current;

    if (!copy_string(out.wifiSsid, sizeof(out.wifiSsid), f.ssid)) {
        return SETTINGS_ERR_SSID;
    }

    // Campo vazio significa "não mexi". O formulário nunca devolve a senha
    // gravada, então não há como distinguir "apaguei" de "não toquei" — e
    // preservar é o comportamento que não tranca o usuário fora da própria rede.
    if (f.pass != NULL && f.pass[0] != '\0') {
        if (!copy_string(out.wifiPass, sizeof(out.wifiPass), f.pass)) {
            return SETTINGS_ERR_PASS;
        }
    }

    if (!copy_string(out.bitaxeHost, sizeof(out.bitaxeHost), f.host)) {
        return SETTINGS_ERR_HOST;
    }

    // Ao contrario da senha, campo vazio aqui significa "desligar o segundo
    // dispositivo" — e um endereco, nao um segredo que a pagina nao devolve.
    if (f.host2 == NULL || f.host2[0] == '\0') {
        out.bitaxeHost2[0] = '\0';
    } else if (!copy_string(out.bitaxeHost2, sizeof(out.bitaxeHost2), f.host2)) {
        return SETTINGS_ERR_HOST2;
    }

    int32_t v = 0;
    if (!parse_range(f.tzMinutes, SETTINGS_TZ_MIN_MINUTES, SETTINGS_TZ_MAX_MINUTES, v)) {
        return SETTINGS_ERR_TZ;
    }
    out.tzMinutes = (int16_t) v;

    if (!parse_range(f.wifiTimeoutMs, SETTINGS_WIFI_TIMEOUT_MIN_MS, SETTINGS_WIFI_TIMEOUT_MAX_MS, v)) {
        return SETTINGS_ERR_WIFI_TIMEOUT;
    }
    out.wifiTimeoutMs = (uint32_t) v;

    if (!parse_range(f.httpTimeoutMs, SETTINGS_HTTP_TIMEOUT_MIN_MS, SETTINGS_HTTP_TIMEOUT_MAX_MS, v)) {
        return SETTINGS_ERR_HTTP_TIMEOUT;
    }
    out.httpTimeoutMs = (uint32_t) v;

    if (!parse_range(f.batteryMinPct, SETTINGS_BATTERY_MIN_PCT, SETTINGS_BATTERY_MAX_PCT, v)) {
        return SETTINGS_ERR_BATTERY;
    }
    out.batteryMinPct = (uint8_t) v;

    if (!parse_range(f.idleRelogioMs, SETTINGS_IDLE_RELOGIO_MIN_MS, SETTINGS_IDLE_RELOGIO_MAX_MS, v)) {
        return SETTINGS_ERR_IDLE_RELOGIO;
    }
    out.idleRelogioMs = (uint32_t) v;

    if (!parse_range(f.idlePainelMs, SETTINGS_IDLE_PAINEL_MIN_MS, SETTINGS_IDLE_PAINEL_MAX_MS, v)) {
        return SETTINGS_ERR_IDLE_PAINEL;
    }
    out.idlePainelMs = (uint32_t) v;

    if (!parse_range(f.brightnessClock, SETTINGS_BRIGHTNESS_MIN_PCT, SETTINGS_BRIGHTNESS_MAX_PCT,
                     v)) {
        return SETTINGS_ERR_BRIGHTNESS;
    }
    out.brightnessClock = (uint8_t) v;

    if (!parse_range(f.brightnessPanel, SETTINGS_BRIGHTNESS_MIN_PCT, SETTINGS_BRIGHTNESS_MAX_PCT,
                     v)) {
        return SETTINGS_ERR_BRIGHTNESS;
    }
    out.brightnessPanel = (uint8_t) v;

    // Faixa larga aqui (0 ate o teto); o "0 ou 5..240" quem cobra e o
    // settings_validate logo abaixo.
    if (!parse_range(f.watchIntervalMin, 0, SETTINGS_WATCH_MAX_MINUTES, v)) {
        return SETTINGS_ERR_WATCH;
    }
    out.watchIntervalMin = (uint16_t) v;

    // Segunda passada: pega o que a faixa não pega, como o passo de 30 min do
    // fuso, e é o único portão que também vale para Settings montado em código.
    return settings_validate(out);
}

bool settings_equal(const Settings &a, const Settings &b)
{
    return strcmp(a.wifiSsid, b.wifiSsid) == 0 && strcmp(a.wifiPass, b.wifiPass) == 0 &&
           strcmp(a.bitaxeHost, b.bitaxeHost) == 0 &&
           strcmp(a.bitaxeHost2, b.bitaxeHost2) == 0 && a.tzMinutes == b.tzMinutes &&
           a.wifiTimeoutMs == b.wifiTimeoutMs && a.httpTimeoutMs == b.httpTimeoutMs &&
           a.batteryMinPct == b.batteryMinPct && a.idleRelogioMs == b.idleRelogioMs &&
           a.idlePainelMs == b.idlePainelMs &&
           a.brightnessClock == b.brightnessClock && a.brightnessPanel == b.brightnessPanel &&
           a.watchIntervalMin == b.watchIntervalMin;
}

uint8_t settings_device_count(const Settings &s)
{
    return s.bitaxeHost2[0] != '\0' ? 2 : 1;
}
