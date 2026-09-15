#pragma once

#include <stdint.h>

// Limites de validação no header porque o formulário do portal, o desenho e os
// testes precisam falar dos MESMOS números. Duplicar isso em três lugares é
// como se planta uma regra que diverge silenciosamente.
#define SETTINGS_SSID_MAX 32
#define SETTINGS_PASS_MAX 63
#define SETTINGS_PASS_MIN 8
#define SETTINGS_HOST_MAX 15

#define SETTINGS_TZ_MIN_MINUTES (-720)
#define SETTINGS_TZ_MAX_MINUTES 840
#define SETTINGS_TZ_STEP_MINUTES 30

#define SETTINGS_WIFI_TIMEOUT_MIN_MS 1000
#define SETTINGS_WIFI_TIMEOUT_MAX_MS 30000
#define SETTINGS_HTTP_TIMEOUT_MIN_MS 1000
#define SETTINGS_HTTP_TIMEOUT_MAX_MS 15000
#define SETTINGS_BATTERY_MIN_PCT 0
#define SETTINGS_BATTERY_MAX_PCT 50
#define SETTINGS_IDLE_RELOGIO_MIN_MS 2000
#define SETTINGS_IDLE_RELOGIO_MAX_MS 60000
#define SETTINGS_IDLE_PAINEL_MIN_MS 5000
#define SETTINGS_IDLE_PAINEL_MAX_MS 120000

#define SETTINGS_BRIGHTNESS_MIN_PCT 10
#define SETTINGS_BRIGHTNESS_MAX_PCT 100

// Zero desliga a vigia. O piso de 5 min existe para impedir uma configuracao
// que drene o aparelho em uma tarde.
#define SETTINGS_WATCH_MIN_MINUTES 5
#define SETTINGS_WATCH_MAX_MINUTES 240

// Todos os campos de tempo são uint32_t: idlePainelMs chega a 120000, que não
// cabe num uint16_t. Uniformizar evita a próxima faixa que cresça sem ninguém
// reparar.
struct Settings
{
    char wifiSsid[SETTINGS_SSID_MAX + 1];
    char wifiPass[SETTINGS_PASS_MAX + 1];
    char bitaxeHost[SETTINGS_HOST_MAX + 1];
    // Vazio significa "um dispositivo so" — nao e erro, e o caso normal.
    char bitaxeHost2[SETTINGS_HOST_MAX + 1];
    int16_t tzMinutes;
    uint32_t wifiTimeoutMs;
    uint32_t httpTimeoutMs;
    uint8_t batteryMinPct;
    uint32_t idleRelogioMs;
    uint32_t idlePainelMs;
    uint8_t brightnessClock;
    uint8_t brightnessPanel;
    uint16_t watchIntervalMin;  // 0 = vigia desligada
};

// O formulário chega do HTTP como texto puro — inclusive os números. Modelar
// isso honestamente põe o parsing dentro do módulo testável, que é onde os
// bugs de "4000abc virou 4000" moram.
struct SettingsForm
{
    const char *ssid;
    const char *pass;  // vazio = manter a senha gravada
    const char *host;
    const char *host2;  // vazio ou NULL desliga o segundo dispositivo
    const char *tzMinutes;
    const char *wifiTimeoutMs;
    const char *httpTimeoutMs;
    const char *batteryMinPct;
    const char *idleRelogioMs;
    const char *idlePainelMs;
    const char *brightnessClock;
    const char *brightnessPanel;
    const char *watchIntervalMin;
};

// SETTINGS_OK é o único valor que autoriza gravar.
enum SettingsError
{
    SETTINGS_OK,
    SETTINGS_ERR_SSID,
    SETTINGS_ERR_PASS,
    SETTINGS_ERR_HOST,
    SETTINGS_ERR_HOST2,
    SETTINGS_ERR_TZ,
    SETTINGS_ERR_WIFI_TIMEOUT,
    SETTINGS_ERR_HTTP_TIMEOUT,
    SETTINGS_ERR_BATTERY,
    SETTINGS_ERR_IDLE_RELOGIO,
    SETTINGS_ERR_IDLE_PAINEL,
    SETTINGS_ERR_BRIGHTNESS,
    SETTINGS_ERR_WATCH,
};

SettingsError settings_validate(const Settings &s);

// Mensagem curta em pt-br, sem acentos, para exibir no formulário.
const char *settings_error_message(SettingsError e);

// Monta `out` a partir de `current` sobrescrito pelos campos de `f`, valida e
// devolve o resultado. Nada é gravado aqui — isso é papel do settings_store.
SettingsError settings_apply_form(const Settings &current, const SettingsForm &f, Settings &out);

// Comparação campo a campo. Existe para o settings_store conferir a gravação
// relendo do NVS; memcmp não serve porque padding de struct não é confiável.
bool settings_equal(const Settings &a, const Settings &b);

// 1 ou 2, conforme o segundo host esteja preenchido. Existe para que a
// contagem seja decidida num lugar so.
uint8_t settings_device_count(const Settings &s);
