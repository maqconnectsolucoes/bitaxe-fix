# Botão de config e portal cativo no T-Watch — Plano de Implementação

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Um botão no rodapé da tela do relógio abre um portal cativo (SoftAP + página web no celular) que grava WiFi, IP do Bitaxe, fuso horário, timeouts e bateria mínima no NVS, eliminando a necessidade de recompilar para reconfigurar.

**Architecture:** A lógica pura (validação, parsing do formulário, teste de acerto do toque, máquina de estados) vive em `lib/bitaxe_core/` porque o PlatformIO só compila `lib/` para os testes. O acesso ao NVS, o servidor web e o desenho ficam em `src/`. `include/config.h` é rebaixado a semente de primeiro boot, lida apenas por `src/settings_store.cpp`.

**Tech Stack:** C++ (Arduino core esp32 2.0.17 via `espressif32@6.9.0`), `Preferences` (NVS), `WebServer`, `DNSServer` e `WiFi` — todos já presentes no core, **nenhuma dependência nova em `platformio.ini`**. Testes em Unity rodando no alvo.

**Spec:** `docs/superpowers/specs/2026-08-10-config-portal-twatch-design.md`

## Global Constraints

- Alvo: TTGO T-Watch 2020 V1, ESP32-D0WDQ6-V3, tela 240x240, PMU AXP202.
- Comentários e strings de UI em pt-br, **sem acentos nas strings desenhadas no TFT** (as fontes do TFT_eSPI não têm os glifos acentuados); comentários no código podem e devem ser acentuados.
- Testes rodam no alvo: `pio test --upload-port COM5 -f <pasta>`. O `--upload-port COM5` é obrigatório e explícito neste projeto.
- Build: `pio run`. Toda tarefa termina com o build compilando.
- Nenhuma entrada nova em `lib_deps`.
- Chaves do NVS têm no máximo 15 caracteres.
- `include/config.h` é gitignored. Nunca commite valores reais; só `include/config.h.example` é versionado.
- Sem suporte a um segundo Bitaxe nesta entrega.

## Estrutura de arquivos

| Arquivo | Responsabilidade | Tarefa |
|---|---|---|
| `lib/bitaxe_core/settings.h/.cpp` | POD `Settings`, validação, parsing do formulário, comparação | 1 |
| `lib/bitaxe_core/ui_layout.h/.cpp` | `clock_hit()` — geometria do botão | 2 |
| `lib/bitaxe_core/state_machine.h/.cpp` | `STATE_CONFIG`, `EVENT_TOUCH_CONFIG`, `IdleTimeouts` | 4 |
| `src/settings_store.h/.cpp` | NVS; único leitor de `config.h` | 3 |
| `src/net.h/.cpp` | WiFi e NTP parametrizados | 5 |
| `src/bitaxe_fetch.h/.cpp` | HTTP parametrizado | 5 |
| `src/snapshot_store.h/.cpp` | `snapshot_clear()` | 5 |
| `src/config_view.h/.cpp` | Desenho da tela de config | 6 |
| `src/clock_view.h/.cpp` | Engrenagem no rodapé | 6 |
| `src/config_portal.h/.cpp` | SoftAP, DNS cativo, formulário, gravação | 7 |
| `src/main.cpp` | Fiação de tudo | 3,4,5,6,7 |
| `include/config.h.example` | Sementes novas | 3 |
| `test/test_settings/test_settings.cpp` | | 1 |
| `test/test_layout/test_layout.cpp` | | 2 |
| `test/test_state/test_state.cpp` | | 4 |
| `README.md` | Documentação | 8 |

---

### Task 1: Settings — validação e formulário (puro)

**Files:**
- Create: `twatch/lib/bitaxe_core/settings.h`
- Create: `twatch/lib/bitaxe_core/settings.cpp`
- Test: `twatch/test/test_settings/test_settings.cpp`

**Interfaces:**
- Consumes: nada.
- Produces: `struct Settings`, `struct SettingsForm`, `enum SettingsError`, `SettingsError settings_validate(const Settings &)`, `SettingsError settings_apply_form(const Settings &current, const SettingsForm &f, Settings &out)`, `const char *settings_error_message(SettingsError)`, `bool settings_equal(const Settings &a, const Settings &b)`, e as macros `SETTINGS_*` de limites.

- [ ] **Step 1: Escrever o header**

Crie `twatch/lib/bitaxe_core/settings.h`:

```cpp
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

// Todos os campos de tempo são uint32_t: idlePainelMs chega a 120000, que não
// cabe num uint16_t. Uniformizar evita a próxima faixa que cresça sem ninguém
// reparar.
struct Settings
{
    char wifiSsid[SETTINGS_SSID_MAX + 1];
    char wifiPass[SETTINGS_PASS_MAX + 1];
    char bitaxeHost[SETTINGS_HOST_MAX + 1];
    int16_t tzMinutes;
    uint32_t wifiTimeoutMs;
    uint32_t httpTimeoutMs;
    uint8_t batteryMinPct;
    uint32_t idleRelogioMs;
    uint32_t idlePainelMs;
};

// O formulário chega do HTTP como texto puro — inclusive os números. Modelar
// isso honestamente põe o parsing dentro do módulo testável, que é onde os
// bugs de "4000abc virou 4000" moram.
struct SettingsForm
{
    const char *ssid;
    const char *pass;  // vazio = manter a senha gravada
    const char *host;
    const char *tzMinutes;
    const char *wifiTimeoutMs;
    const char *httpTimeoutMs;
    const char *batteryMinPct;
    const char *idleRelogioMs;
    const char *idlePainelMs;
};

// SETTINGS_OK é o único valor que autoriza gravar.
enum SettingsError
{
    SETTINGS_OK,
    SETTINGS_ERR_SSID,
    SETTINGS_ERR_PASS,
    SETTINGS_ERR_HOST,
    SETTINGS_ERR_TZ,
    SETTINGS_ERR_WIFI_TIMEOUT,
    SETTINGS_ERR_HTTP_TIMEOUT,
    SETTINGS_ERR_BATTERY,
    SETTINGS_ERR_IDLE_RELOGIO,
    SETTINGS_ERR_IDLE_PAINEL,
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
```

- [ ] **Step 2: Escrever o teste que falha**

Crie `twatch/test/test_settings/test_settings.cpp`:

```cpp
#include <Arduino.h>
#include <string.h>
#include <unity.h>

#include "settings.h"

// Configuração plausível e válida; cada teste estraga UM campo a partir dela.
static Settings baseline(void)
{
    Settings s = {};
    strcpy(s.wifiSsid, "minha-rede");
    strcpy(s.wifiPass, "segredo123");
    strcpy(s.bitaxeHost, "192.168.1.100");
    s.tzMinutes = -180;
    s.wifiTimeoutMs = 8000;
    s.httpTimeoutMs = 4000;
    s.batteryMinPct = 15;
    s.idleRelogioMs = 5000;
    s.idlePainelMs = 15000;
    return s;
}

static SettingsForm baselineForm(void)
{
    SettingsForm f;
    f.ssid = "outra-rede";
    f.pass = "";
    f.host = "10.0.0.7";
    f.tzMinutes = "-180";
    f.wifiTimeoutMs = "8000";
    f.httpTimeoutMs = "4000";
    f.batteryMinPct = "15";
    f.idleRelogioMs = "5000";
    f.idlePainelMs = "15000";
    return f;
}

void test_baseline_e_valido(void)
{
    TEST_ASSERT_EQUAL(SETTINGS_OK, settings_validate(baseline()));
}

void test_ssid_vazio_reprova(void)
{
    Settings s = baseline();
    s.wifiSsid[0] = '\0';
    TEST_ASSERT_EQUAL(SETTINGS_ERR_SSID, settings_validate(s));
}

void test_senha_de_sete_caracteres_reprova(void)
{
    // WPA2 não aceita 1..7; deixar passar viraria um AP que nunca conecta.
    Settings s = baseline();
    strcpy(s.wifiPass, "1234567");
    TEST_ASSERT_EQUAL(SETTINGS_ERR_PASS, settings_validate(s));
}

void test_senha_vazia_aprova_rede_aberta(void)
{
    Settings s = baseline();
    s.wifiPass[0] = '\0';
    TEST_ASSERT_EQUAL(SETTINGS_OK, settings_validate(s));
}

void test_host_com_nome_reprova(void)
{
    // bitaxe.local resolve por IPv6 e a API devolve 401. Aceitar hostname aqui
    // seria plantar esse bug de propósito.
    Settings s = baseline();
    strcpy(s.bitaxeHost, "bitaxe.local");
    TEST_ASSERT_EQUAL(SETTINGS_ERR_HOST, settings_validate(s));
}

void test_host_com_octeto_acima_de_255_reprova(void)
{
    Settings s = baseline();
    strcpy(s.bitaxeHost, "192.168.1.256");
    TEST_ASSERT_EQUAL(SETTINGS_ERR_HOST, settings_validate(s));
}

void test_host_incompleto_reprova(void)
{
    Settings s = baseline();
    strcpy(s.bitaxeHost, "192.168.1");
    TEST_ASSERT_EQUAL(SETTINGS_ERR_HOST, settings_validate(s));
}

void test_fuso_fora_da_faixa_reprova(void)
{
    Settings s = baseline();
    s.tzMinutes = 900;
    TEST_ASSERT_EQUAL(SETTINGS_ERR_TZ, settings_validate(s));
}

void test_fuso_fora_do_passo_reprova(void)
{
    Settings s = baseline();
    s.tzMinutes = -190;
    TEST_ASSERT_EQUAL(SETTINGS_ERR_TZ, settings_validate(s));
}

void test_bateria_acima_de_cinquenta_reprova(void)
{
    // Acima disso o painel nunca abriria.
    Settings s = baseline();
    s.batteryMinPct = 51;
    TEST_ASSERT_EQUAL(SETTINGS_ERR_BATTERY, settings_validate(s));
}

void test_ociosidade_do_painel_acima_do_teto_reprova(void)
{
    Settings s = baseline();
    s.idlePainelMs = 130000;
    TEST_ASSERT_EQUAL(SETTINGS_ERR_IDLE_PAINEL, settings_validate(s));
}

void test_form_senha_vazia_preserva_a_gravada(void)
{
    Settings out = {};
    TEST_ASSERT_EQUAL(SETTINGS_OK, settings_apply_form(baseline(), baselineForm(), out));
    TEST_ASSERT_EQUAL_STRING("segredo123", out.wifiPass);
    TEST_ASSERT_EQUAL_STRING("outra-rede", out.wifiSsid);
    TEST_ASSERT_EQUAL_STRING("10.0.0.7", out.bitaxeHost);
}

void test_form_senha_preenchida_substitui(void)
{
    SettingsForm f = baselineForm();
    f.pass = "novasenha";
    Settings out = {};
    TEST_ASSERT_EQUAL(SETTINGS_OK, settings_apply_form(baseline(), f, out));
    TEST_ASSERT_EQUAL_STRING("novasenha", out.wifiPass);
}

void test_form_numero_com_lixo_reprova(void)
{
    // strtol sozinho leria "4000abc" como 4000. O parser precisa ser estrito.
    SettingsForm f = baselineForm();
    f.httpTimeoutMs = "4000abc";
    Settings out = {};
    TEST_ASSERT_EQUAL(SETTINGS_ERR_HTTP_TIMEOUT, settings_apply_form(baseline(), f, out));
}

void test_form_numero_vazio_reprova(void)
{
    SettingsForm f = baselineForm();
    f.wifiTimeoutMs = "";
    Settings out = {};
    TEST_ASSERT_EQUAL(SETTINGS_ERR_WIFI_TIMEOUT, settings_apply_form(baseline(), f, out));
}

void test_form_ssid_longo_demais_reprova(void)
{
    // 33 caracteres: recusa em vez de truncar em silêncio.
    SettingsForm f = baselineForm();
    f.ssid = "123456789012345678901234567890123";
    Settings out = {};
    TEST_ASSERT_EQUAL(SETTINGS_ERR_SSID, settings_apply_form(baseline(), f, out));
}

void test_form_fuso_fora_do_passo_reprova(void)
{
    SettingsForm f = baselineForm();
    f.tzMinutes = "-190";
    Settings out = {};
    TEST_ASSERT_EQUAL(SETTINGS_ERR_TZ, settings_apply_form(baseline(), f, out));
}

void test_equal_detecta_diferenca_em_cada_campo(void)
{
    Settings a = baseline();
    TEST_ASSERT_TRUE(settings_equal(a, baseline()));

    Settings b = baseline();
    strcpy(b.bitaxeHost, "10.0.0.1");
    TEST_ASSERT_FALSE(settings_equal(a, b));

    Settings c = baseline();
    c.tzMinutes = 0;
    TEST_ASSERT_FALSE(settings_equal(a, c));

    Settings d = baseline();
    d.idlePainelMs = 20000;
    TEST_ASSERT_FALSE(settings_equal(a, d));
}

void setUp(void) {}
void tearDown(void) {}

// Testes no alvo usam setup()/loop(), não main().
void setup()
{
    delay(2000);
    UNITY_BEGIN();
    RUN_TEST(test_baseline_e_valido);
    RUN_TEST(test_ssid_vazio_reprova);
    RUN_TEST(test_senha_de_sete_caracteres_reprova);
    RUN_TEST(test_senha_vazia_aprova_rede_aberta);
    RUN_TEST(test_host_com_nome_reprova);
    RUN_TEST(test_host_com_octeto_acima_de_255_reprova);
    RUN_TEST(test_host_incompleto_reprova);
    RUN_TEST(test_fuso_fora_da_faixa_reprova);
    RUN_TEST(test_fuso_fora_do_passo_reprova);
    RUN_TEST(test_bateria_acima_de_cinquenta_reprova);
    RUN_TEST(test_ociosidade_do_painel_acima_do_teto_reprova);
    RUN_TEST(test_form_senha_vazia_preserva_a_gravada);
    RUN_TEST(test_form_senha_preenchida_substitui);
    RUN_TEST(test_form_numero_com_lixo_reprova);
    RUN_TEST(test_form_numero_vazio_reprova);
    RUN_TEST(test_form_ssid_longo_demais_reprova);
    RUN_TEST(test_form_fuso_fora_do_passo_reprova);
    RUN_TEST(test_equal_detecta_diferenca_em_cada_campo);
    UNITY_END();
}

void loop() {}
```

- [ ] **Step 3: Rodar o teste para ver falhar**

Run: `cd twatch && pio test --upload-port COM5 -f test_settings`
Expected: FALHA de compilação — `settings.cpp` não existe, os símbolos `settings_validate`, `settings_apply_form` e `settings_equal` não resolvem no link.

- [ ] **Step 4: Implementar**

Crie `twatch/lib/bitaxe_core/settings.cpp`:

```cpp
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

    return SETTINGS_OK;
}

const char *settings_error_message(SettingsError e)
{
    switch (e) {
    case SETTINGS_OK: return "";
    case SETTINGS_ERR_SSID: return "Nome da rede: 1 a 32 caracteres.";
    case SETTINGS_ERR_PASS: return "Senha: deixe em branco (rede aberta) ou use de 8 a 63 caracteres.";
    case SETTINGS_ERR_HOST: return "IP do Bitaxe: use o IPv4 do aparelho, por exemplo 192.168.1.100.";
    case SETTINGS_ERR_TZ: return "Fuso invalido.";
    case SETTINGS_ERR_WIFI_TIMEOUT: return "Timeout de WiFi: 1000 a 30000 ms.";
    case SETTINGS_ERR_HTTP_TIMEOUT: return "Timeout de HTTP: 1000 a 15000 ms.";
    case SETTINGS_ERR_BATTERY: return "Bateria minima: 0 a 50 por cento.";
    case SETTINGS_ERR_IDLE_RELOGIO: return "Ociosidade do relogio: 2000 a 60000 ms.";
    case SETTINGS_ERR_IDLE_PAINEL: return "Ociosidade do painel: 5000 a 120000 ms.";
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

    // Segunda passada: pega o que a faixa não pega, como o passo de 30 min do
    // fuso, e é o único portão que também vale para Settings montado em código.
    return settings_validate(out);
}

bool settings_equal(const Settings &a, const Settings &b)
{
    return strcmp(a.wifiSsid, b.wifiSsid) == 0 && strcmp(a.wifiPass, b.wifiPass) == 0 &&
           strcmp(a.bitaxeHost, b.bitaxeHost) == 0 && a.tzMinutes == b.tzMinutes &&
           a.wifiTimeoutMs == b.wifiTimeoutMs && a.httpTimeoutMs == b.httpTimeoutMs &&
           a.batteryMinPct == b.batteryMinPct && a.idleRelogioMs == b.idleRelogioMs &&
           a.idlePainelMs == b.idlePainelMs;
}
```

- [ ] **Step 5: Rodar o teste para ver passar**

Run: `cd twatch && pio test --upload-port COM5 -f test_settings`
Expected: PASS, 18 testes.

- [ ] **Step 6: Confirmar que o firmware ainda compila**

Run: `cd twatch && pio run`
Expected: SUCCESS. Nada em `src/` mudou ainda.

- [ ] **Step 7: Commit**

```bash
git add twatch/lib/bitaxe_core/settings.h twatch/lib/bitaxe_core/settings.cpp twatch/test/test_settings/test_settings.cpp
git commit -m "feat: add Settings with strict validation and form parsing"
```

---

### Task 2: Geometria do botão

**Files:**
- Create: `twatch/lib/bitaxe_core/ui_layout.h`
- Create: `twatch/lib/bitaxe_core/ui_layout.cpp`
- Test: `twatch/test/test_layout/test_layout.cpp`

**Interfaces:**
- Consumes: nada.
- Produces: `enum ClockHit { HIT_PAINEL, HIT_CONFIG }`, `ClockHit clock_hit(int16_t x, int16_t y)`, macros `SCREEN_W`, `SCREEN_H`, `CONFIG_STRIP_TOP_Y`.

- [ ] **Step 1: Escrever o teste que falha**

Crie `twatch/test/test_layout/test_layout.cpp`:

```cpp
#include <Arduino.h>
#include <unity.h>

#include "ui_layout.h"

void test_toque_no_rodape_abre_config(void)
{
    TEST_ASSERT_EQUAL(HIT_CONFIG, clock_hit(120, 220));
}

void test_toque_no_meio_abre_painel(void)
{
    TEST_ASSERT_EQUAL(HIT_PAINEL, clock_hit(120, 100));
}

void test_fronteira_da_faixa(void)
{
    // 192 é a primeira linha da faixa; 191 é a última linha fora dela.
    TEST_ASSERT_EQUAL(HIT_CONFIG, clock_hit(120, CONFIG_STRIP_TOP_Y));
    TEST_ASSERT_EQUAL(HIT_PAINEL, clock_hit(120, CONFIG_STRIP_TOP_Y - 1));
}

void test_faixa_cobre_a_largura_inteira(void)
{
    TEST_ASSERT_EQUAL(HIT_CONFIG, clock_hit(0, 230));
    TEST_ASSERT_EQUAL(HIT_CONFIG, clock_hit(SCREEN_W - 1, 230));
}

void setUp(void) {}
void tearDown(void) {}

void setup()
{
    delay(2000);
    UNITY_BEGIN();
    RUN_TEST(test_toque_no_rodape_abre_config);
    RUN_TEST(test_toque_no_meio_abre_painel);
    RUN_TEST(test_fronteira_da_faixa);
    RUN_TEST(test_faixa_cobre_a_largura_inteira);
    UNITY_END();
}

void loop() {}
```

- [ ] **Step 2: Rodar para ver falhar**

Run: `cd twatch && pio test --upload-port COM5 -f test_layout`
Expected: FALHA — `ui_layout.h` não existe.

- [ ] **Step 3: Implementar**

Crie `twatch/lib/bitaxe_core/ui_layout.h`:

```cpp
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
```

Crie `twatch/lib/bitaxe_core/ui_layout.cpp`:

```cpp
#include "ui_layout.h"

ClockHit clock_hit(int16_t x, int16_t y)
{
    // A faixa cobre a largura inteira: x não entra na decisão hoje, mas fica na
    // assinatura porque é o par natural do que o touch entrega, e um segundo
    // botão no rodapé só mexeria aqui.
    (void) x;
    return y >= CONFIG_STRIP_TOP_Y ? HIT_CONFIG : HIT_PAINEL;
}
```

- [ ] **Step 4: Rodar para ver passar**

Run: `cd twatch && pio test --upload-port COM5 -f test_layout`
Expected: PASS, 4 testes.

- [ ] **Step 5: Commit**

```bash
git add twatch/lib/bitaxe_core/ui_layout.h twatch/lib/bitaxe_core/ui_layout.cpp twatch/test/test_layout/test_layout.cpp
git commit -m "feat: add clock_hit for the config button strip"
```

---

### Task 3: Persistência no NVS

**Files:**
- Create: `twatch/src/settings_store.h`
- Create: `twatch/src/settings_store.cpp`
- Modify: `twatch/include/config.h.example`
- Modify: `twatch/include/config.h` (local, gitignored)
- Modify: `twatch/src/main.cpp`

**Interfaces:**
- Consumes: `Settings`, `settings_equal()` (Task 1).
- Produces: `void settings_load(Settings &out)`, `bool settings_save(const Settings &s)`.

- [ ] **Step 1: Acrescentar as sementes novas ao config.h.example**

Em `twatch/include/config.h.example`, adicione ao final:

```c
// Semente do primeiro boot. Depois que o portal gravar no NVS, estes valores
// deixam de ser consultados — só voltam a valer se o NVS for apagado.
#define TZ_MINUTES (-180)     // -180 = UTC-3, horario de Brasilia
#define IDLE_RELOGIO_MS 5000
#define IDLE_PAINEL_MS 15000
```

- [ ] **Step 2: Acrescentar as mesmas linhas ao seu config.h local**

`twatch/include/config.h` é gitignored e não é atualizado sozinho. Copie as três
linhas do passo anterior para lá também — sem isso o build falha com
`'TZ_MINUTES' was not declared`.

- [ ] **Step 3: Escrever o header**

Crie `twatch/src/settings_store.h`:

```cpp
#pragma once

#include "settings.h"

// Lê do NVS. Campo ausente cai no valor de semente do config.h — é assim que
// "primeiro boot funciona sem passar pelo portal" sai de graça, campo a campo.
void settings_load(Settings &out);

// Grava e confere relendo. false significa que nada confiável foi gravado.
bool settings_save(const Settings &s);
```

- [ ] **Step 4: Implementar**

Crie `twatch/src/settings_store.cpp`:

```cpp
#include "settings_store.h"

#include <Preferences.h>

#include "config.h"

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

    out.tzMinutes = (int16_t) p.getInt("tz", TZ_MINUTES);
    out.wifiTimeoutMs = p.getUInt("wifims", WIFI_CONNECT_TIMEOUT_MS);
    out.httpTimeoutMs = p.getUInt("httpms", HTTP_TIMEOUT_MS);
    out.batteryMinPct = p.getUChar("battmin", BATTERY_MIN_PERCENT);
    out.idleRelogioMs = p.getUInt("idlerel", IDLE_RELOGIO_MS);
    out.idlePainelMs = p.getUInt("idlepan", IDLE_PAINEL_MS);

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
    p.putInt("tz", s.tzMinutes);
    p.putUInt("wifims", s.wifiTimeoutMs);
    p.putUInt("httpms", s.httpTimeoutMs);
    p.putUChar("battmin", s.batteryMinPct);
    p.putUInt("idlerel", s.idleRelogioMs);
    p.putUInt("idlepan", s.idlePainelMs);
    p.end();

    // Confere relendo, em vez de somar os retornos de putX. Motivo concreto:
    // Preferences::putString devolve strlen(value), então gravar senha vazia
    // devolve 0 — indistinguível de erro. Reler resolve isso e ainda cobre
    // truncamento e chave que não coube.
    Settings check = {};
    settings_load(check);
    return settings_equal(check, s);
}
```

- [ ] **Step 5: Carregar as Settings no boot e imprimir no serial**

Em `twatch/src/main.cpp`, adicione ao bloco de includes:

```cpp
#include "settings_store.h"
```

Declare o estado global logo abaixo de `static uint32_t lastInteraction = 0;`:

```cpp
static Settings g_settings;
```

E no `setup()`, imediatamente depois de `watch->openBL();`, insira:

```cpp
    settings_load(g_settings);
    Serial.printf("config: rede=%s host=%s tz=%d\n", g_settings.wifiSsid,
                  g_settings.bitaxeHost, (int) g_settings.tzMinutes);
```

Ainda ninguém consome `g_settings` — as tarefas 4 e 5 fazem isso. O objetivo
deste passo é provar, com o log, que a leitura do NVS funciona.

- [ ] **Step 6: Compilar e gravar**

Run: `cd twatch && pio run -t upload --upload-port COM5 && pio device monitor`
Expected: no boot, a linha `config: rede=<seu ssid> host=<seu ip> tz=-180` com os
valores do seu `config.h`, provando que o NVS vazio caiu nos defaults.

- [ ] **Step 7: Commit**

```bash
git add twatch/src/settings_store.h twatch/src/settings_store.cpp twatch/include/config.h.example twatch/src/main.cpp
git commit -m "feat: persist settings in NVS seeded from config.h"
```

---

### Task 4: STATE_CONFIG na máquina de estados

**Files:**
- Modify: `twatch/lib/bitaxe_core/state_machine.h`
- Modify: `twatch/lib/bitaxe_core/state_machine.cpp`
- Modify: `twatch/test/test_state/test_state.cpp`
- Modify: `twatch/src/main.cpp`

**Interfaces:**
- Consumes: `Settings` (Task 1), `g_settings` (Task 3).
- Produces: `STATE_CONFIG`, `EVENT_TOUCH_CONFIG`, `IDLE_TIMEOUT_CONFIG_MS`, `struct IdleTimeouts { uint32_t relogioMs; uint32_t painelMs; }`, `AppState state_next(AppState, AppEvent, uint32_t idleMs, const IdleTimeouts &t)`.

- [ ] **Step 1: Reescrever o teste**

Substitua o conteúdo de `twatch/test/test_state/test_state.cpp`:

```cpp
#include <Arduino.h>
#include <unity.h>

#include "state_machine.h"

// Os valores que eram constantes de compilação passaram a vir das Settings.
// Os testes fixam os de fábrica para continuar verificando o mesmo contrato.
static const IdleTimeouts T = {5000, 15000};

void test_wake_from_sleep_goes_to_clock(void)
{
    TEST_ASSERT_EQUAL(STATE_RELOGIO, state_next(STATE_DEEP_SLEEP, EVENT_WAKE, 0, T));
}

void test_touch_on_clock_opens_panel(void)
{
    TEST_ASSERT_EQUAL(STATE_PAINEL, state_next(STATE_RELOGIO, EVENT_TOUCH, 0, T));
}

void test_touch_on_panel_returns_to_clock(void)
{
    TEST_ASSERT_EQUAL(STATE_RELOGIO, state_next(STATE_PAINEL, EVENT_TOUCH, 0, T));
}

void test_clock_sleeps_after_five_seconds_idle(void)
{
    TEST_ASSERT_EQUAL(STATE_RELOGIO, state_next(STATE_RELOGIO, EVENT_TICK, 4999, T));
    TEST_ASSERT_EQUAL(STATE_DEEP_SLEEP, state_next(STATE_RELOGIO, EVENT_TICK, 5000, T));
}

void test_panel_sleeps_after_fifteen_seconds_idle(void)
{
    TEST_ASSERT_EQUAL(STATE_PAINEL, state_next(STATE_PAINEL, EVENT_TICK, 14999, T));
    TEST_ASSERT_EQUAL(STATE_DEEP_SLEEP, state_next(STATE_PAINEL, EVENT_TICK, 15000, T));
}

void test_tick_never_wakes_from_sleep(void)
{
    // Só um evento externo acorda; o tempo passando, não.
    TEST_ASSERT_EQUAL(STATE_DEEP_SLEEP, state_next(STATE_DEEP_SLEEP, EVENT_TICK, 999999, T));
}

void test_timeouts_vem_do_parametro_nao_de_constante(void)
{
    // Prova que os tempos são mesmo configuráveis: com um teto maior, os
    // mesmos 5000 ms deixam de dormir.
    const IdleTimeouts longo = {30000, 60000};
    TEST_ASSERT_EQUAL(STATE_RELOGIO, state_next(STATE_RELOGIO, EVENT_TICK, 5000, longo));
}

void test_touch_no_rodape_abre_config(void)
{
    TEST_ASSERT_EQUAL(STATE_CONFIG, state_next(STATE_RELOGIO, EVENT_TOUCH_CONFIG, 0, T));
}

void test_touch_na_config_volta_ao_relogio(void)
{
    TEST_ASSERT_EQUAL(STATE_RELOGIO, state_next(STATE_CONFIG, EVENT_TOUCH, 0, T));
}

void test_config_dorme_apos_tres_minutos(void)
{
    TEST_ASSERT_EQUAL(STATE_CONFIG, state_next(STATE_CONFIG, EVENT_TICK, 179000, T));
    TEST_ASSERT_EQUAL(STATE_DEEP_SLEEP, state_next(STATE_CONFIG, EVENT_TICK, 180000, T));
}

void test_touch_config_no_painel_volta_ao_relogio(void)
{
    // O painel não tem engrenagem. Tratar os dois toques igual mantém a função
    // total, sem entrada que caia no default silencioso.
    TEST_ASSERT_EQUAL(STATE_RELOGIO, state_next(STATE_PAINEL, EVENT_TOUCH_CONFIG, 0, T));
}

void setUp(void) {}
void tearDown(void) {}

// Testes no alvo usam setup()/loop(), não main().
void setup()
{
    delay(2000);
    UNITY_BEGIN();
    RUN_TEST(test_wake_from_sleep_goes_to_clock);
    RUN_TEST(test_touch_on_clock_opens_panel);
    RUN_TEST(test_touch_on_panel_returns_to_clock);
    RUN_TEST(test_clock_sleeps_after_five_seconds_idle);
    RUN_TEST(test_panel_sleeps_after_fifteen_seconds_idle);
    RUN_TEST(test_tick_never_wakes_from_sleep);
    RUN_TEST(test_timeouts_vem_do_parametro_nao_de_constante);
    RUN_TEST(test_touch_no_rodape_abre_config);
    RUN_TEST(test_touch_na_config_volta_ao_relogio);
    RUN_TEST(test_config_dorme_apos_tres_minutos);
    RUN_TEST(test_touch_config_no_painel_volta_ao_relogio);
    UNITY_END();
}

void loop() {}
```

- [ ] **Step 2: Rodar para ver falhar**

Run: `cd twatch && pio test --upload-port COM5 -f test_state`
Expected: FALHA de compilação — `STATE_CONFIG`, `EVENT_TOUCH_CONFIG` e `IdleTimeouts` não existem, e `state_next` recebe 3 argumentos.

- [ ] **Step 3: Reescrever o header**

Substitua o conteúdo de `twatch/lib/bitaxe_core/state_machine.h`:

```cpp
#pragma once

#include <stdint.h>

// Teto de vida do portal, e por tabela do SoftAP. É constante de compilação de
// propósito: é uma trava de segurança de bateria, e não faria sentido o usuário
// poder afrouxá-la pelo próprio portal.
#define IDLE_TIMEOUT_CONFIG_MS 180000

enum AppState
{
    STATE_DEEP_SLEEP,
    STATE_RELOGIO,
    STATE_PAINEL,
    STATE_CONFIG,
};

enum AppEvent
{
    EVENT_WAKE,          // despertou do deep sleep (toque ou botão PEK)
    EVENT_TOUCH,         // toque com a tela já acesa
    EVENT_TOUCH_CONFIG,  // toque na faixa da engrenagem, no rodapé do relógio
    EVENT_TICK,          // passagem de tempo
};

// Os tempos de ociosidade do relógio e do painel deixaram de ser constantes:
// vêm das Settings, gravadas pelo portal.
struct IdleTimeouts
{
    uint32_t relogioMs;
    uint32_t painelMs;
};

// Pura: recebe estado, evento, tempo ocioso e os tetos, devolve o próximo estado.
AppState state_next(AppState current, AppEvent event, uint32_t idleMs, const IdleTimeouts &t);
```

- [ ] **Step 4: Reescrever a implementação**

Substitua o conteúdo de `twatch/lib/bitaxe_core/state_machine.cpp`:

```cpp
#include "state_machine.h"

AppState state_next(AppState current, AppEvent event, uint32_t idleMs, const IdleTimeouts &t)
{
    switch (current) {
    case STATE_DEEP_SLEEP:
        return event == EVENT_WAKE ? STATE_RELOGIO : STATE_DEEP_SLEEP;

    case STATE_RELOGIO:
        if (event == EVENT_TOUCH_CONFIG) {
            return STATE_CONFIG;
        }
        if (event == EVENT_TOUCH) {
            return STATE_PAINEL;
        }
        return idleMs >= t.relogioMs ? STATE_DEEP_SLEEP : STATE_RELOGIO;

    case STATE_PAINEL:
        // O painel não tem engrenagem; tratar os dois toques igual mantém a
        // função total.
        if (event == EVENT_TOUCH || event == EVENT_TOUCH_CONFIG) {
            return STATE_RELOGIO;
        }
        return idleMs >= t.painelMs ? STATE_DEEP_SLEEP : STATE_PAINEL;

    case STATE_CONFIG:
        if (event == EVENT_TOUCH || event == EVENT_TOUCH_CONFIG) {
            return STATE_RELOGIO;
        }
        return idleMs >= IDLE_TIMEOUT_CONFIG_MS ? STATE_DEEP_SLEEP : STATE_CONFIG;
    }
    return STATE_DEEP_SLEEP;
}
```

- [ ] **Step 5: Atualizar as chamadas em main.cpp**

Em `twatch/src/main.cpp`, acrescente esta função auxiliar logo abaixo da
declaração de `g_settings`:

```cpp
// Os tetos de ociosidade vêm das Settings a cada chamada, não de uma cópia
// guardada: assim um valor recém-gravado passa a valer sem caminho extra.
static IdleTimeouts idleTimeouts()
{
    IdleTimeouts t;
    t.relogioMs = g_settings.idleRelogioMs;
    t.painelMs = g_settings.idlePainelMs;
    return t;
}
```

Troque as três chamadas existentes de `state_next`:

- em `setup()`: `state = state_next(STATE_DEEP_SLEEP, EVENT_WAKE, 0, idleTimeouts());`
- no bloco de toque de `loop()`: `state = state_next(state, EVENT_TOUCH, 0, idleTimeouts());`
- no tick de `loop()`: `state = state_next(state, EVENT_TICK, millis() - lastInteraction, idleTimeouts());`

- [ ] **Step 6: Rodar os testes para ver passar**

Run: `cd twatch && pio test --upload-port COM5 -f test_state`
Expected: PASS, 11 testes.

- [ ] **Step 7: Confirmar o build**

Run: `cd twatch && pio run`
Expected: SUCCESS.

- [ ] **Step 8: Commit**

```bash
git add twatch/lib/bitaxe_core/state_machine.h twatch/lib/bitaxe_core/state_machine.cpp twatch/test/test_state/test_state.cpp twatch/src/main.cpp
git commit -m "feat: add STATE_CONFIG and runtime idle timeouts"
```

---

### Task 5: Parametrizar rede, HTTP e cache

**Files:**
- Modify: `twatch/src/net.h`, `twatch/src/net.cpp`
- Modify: `twatch/src/bitaxe_fetch.h`, `twatch/src/bitaxe_fetch.cpp`
- Modify: `twatch/src/snapshot_store.h`, `twatch/src/snapshot_store.cpp`
- Modify: `twatch/src/main.cpp`

**Interfaces:**
- Consumes: `Settings`, `g_settings` (Tasks 1 e 3).
- Produces: `bool net_connect(const char *ssid, const char *pass, uint32_t timeoutMs)`, `bool net_sync_time(int16_t tzMinutes)`, `FetchResult bitaxe_fetch(const char *host, uint32_t timeoutMs, BitaxeStatus &out, int &httpCode)`, `void snapshot_clear()`.

- [ ] **Step 1: Parametrizar net**

Em `twatch/src/net.h`, troque as duas assinaturas:

```cpp
bool net_connect(const char *ssid, const char *pass, uint32_t timeoutMs);
void net_disconnect();
bool net_sync_time(int16_t tzMinutes);
```

Substitua o conteúdo de `twatch/src/net.cpp`:

```cpp
#include "net.h"

#include <WiFi.h>
#include <time.h>

#include "config.h"

bool net_connect(const char *ssid, const char *pass, uint32_t timeoutMs)
{
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, pass);

    const uint32_t deadline = millis() + timeoutMs;
    while (WiFi.status() != WL_CONNECTED && millis() < deadline) {
        delay(100);
    }
    return WiFi.status() == WL_CONNECTED;
}

void net_disconnect()
{
    // O rádio é o maior consumidor: desligar antes de dormir é o ponto todo.
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
}

bool net_sync_time(int16_t tzMinutes)
{
    // Era configTime(0, ...), ou seja, UTC — o relógio mostrava três horas a
    // mais que Brasília. O offset agora vem das Settings.
    configTime((long) tzMinutes * 60, 0, NTP_SERVER);
    struct tm t;
    return getLocalTime(&t, 5000);
}
```

`NTP_SERVER` continua vindo do `config.h`: não é configurável pelo portal.

- [ ] **Step 2: Parametrizar bitaxe_fetch**

Em `twatch/src/bitaxe_fetch.h`, troque a assinatura de `bitaxe_fetch` por:

```cpp
FetchResult bitaxe_fetch(const char *host, uint32_t timeoutMs, BitaxeStatus &out, int &httpCode);
```

Em `twatch/src/bitaxe_fetch.cpp`, remova `#include "config.h"` e substitua o
corpo da função por:

```cpp
FetchResult bitaxe_fetch(const char *host, uint32_t timeoutMs, BitaxeStatus &out, int &httpCode)
{
    httpCode = 0;

    if (WiFi.status() != WL_CONNECTED) {
        return FETCH_NO_NETWORK;
    }

    const String url = String("http://") + host + "/api/system/info";

    HTTPClient http;
    http.setConnectTimeout(timeoutMs);
    http.setTimeout(timeoutMs);

    if (!http.begin(url)) {
        return FETCH_NO_RESPONSE;
    }

    // O ESP-Miner valida private-network CORS: sem o header Origin a resposta
    // é 401, mesmo com IP correto e o dispositivo saudável.
    http.addHeader("Origin", String("http://") + host);

    httpCode = http.GET();
    if (httpCode != HTTP_CODE_OK) {
        http.end();
        return httpCode <= 0 ? FETCH_NO_RESPONSE : FETCH_BAD_STATUS;
    }

    const String body = http.getString();
    http.end();

    return bitaxe_parse(body.c_str(), out) ? FETCH_OK : FETCH_BAD_JSON;
}
```

- [ ] **Step 3: Acrescentar snapshot_clear**

Em `twatch/src/snapshot_store.h`, adicione ao final:

```cpp
// Invalida o cache. Chamado quando a configuração muda: se o IP do Bitaxe ou o
// fuso mudaram, o dado guardado passou a se referir a outra coisa.
void snapshot_clear();
```

Em `twatch/src/snapshot_store.cpp`, adicione ao final:

```cpp
void snapshot_clear()
{
    // Zerar o magic basta: é ele que snapshot_load e snapshot_age conferem.
    g_magic = 0;
    g_snapshotEpoch = 0;
}
```

- [ ] **Step 4: Atualizar as chamadas em main.cpp**

Em `enterPanel()`, troque a checagem de bateria e as três chamadas de rede:

```cpp
    const int battery = watch->power->getBattPercentage();
    if (battery >= 0 && battery < (int) g_settings.batteryMinPct) {
```

```cpp
    if (!net_connect(g_settings.wifiSsid, g_settings.wifiPass, g_settings.wifiTimeoutMs)) {
```

```cpp
    if (!g_timeIsValid && net_sync_time(g_settings.tzMinutes)) {
```

```cpp
    const FetchResult r = bitaxe_fetch(g_settings.bitaxeHost, g_settings.httpTimeoutMs, fresh, httpCode);
```

- [ ] **Step 5: Compilar e gravar**

Run: `cd twatch && pio run -t upload --upload-port COM5 && pio device monitor`
Expected: SUCCESS. Abra o painel no relógio: ele conecta, busca e desenha como
antes. Com `TZ_MINUTES` valendo −180, a hora no relógio passa a ser a de
Brasília — antes vinha três horas adiantada.

- [ ] **Step 6: Commit**

```bash
git add twatch/src/net.h twatch/src/net.cpp twatch/src/bitaxe_fetch.h twatch/src/bitaxe_fetch.cpp twatch/src/snapshot_store.h twatch/src/snapshot_store.cpp twatch/src/main.cpp
git commit -m "feat: drive network, HTTP and timezone from runtime settings"
```

---

### Task 6: Botão na tela e tela de config

Ao fim desta tarefa o botão funciona e a tela de config aparece, **mas o portal
ainda não sobe** — a tela mostra os dados do AP e o contador. É deliberado: dá
para validar botão, transição, contador e saída sem o servidor no caminho.

**Files:**
- Create: `twatch/src/config_view.h`, `twatch/src/config_view.cpp`
- Modify: `twatch/src/clock_view.cpp`
- Modify: `twatch/src/main.cpp`

**Interfaces:**
- Consumes: `clock_hit()`, `CONFIG_STRIP_TOP_Y` (Task 2); `STATE_CONFIG`, `EVENT_TOUCH_CONFIG`, `IDLE_TIMEOUT_CONFIG_MS` (Task 4).
- Produces: `void config_view_draw(TTGOClass *watch, const char *apSsid, const char *apPass, const char *url, uint32_t secondsLeft, bool lowBattery)`, `void config_view_tick(TTGOClass *watch, uint32_t secondsLeft)`, `void config_view_message(TTGOClass *watch, const char *line1, const char *line2)`.

- [ ] **Step 1: Escrever o header da tela de config**

Crie `twatch/src/config_view.h`:

```cpp
#pragma once

#include <LilyGoWatch.h>

// Tela cheia do modo de configuração.
void config_view_draw(TTGOClass *watch, const char *apSsid, const char *apPass,
                      const char *url, uint32_t secondsLeft, bool lowBattery);

// Redesenha só a linha do contador, uma vez por segundo.
void config_view_tick(TTGOClass *watch, uint32_t secondsLeft);

// Aviso de duas linhas ("salvo"/"reiniciando", falha ao subir o AP).
void config_view_message(TTGOClass *watch, const char *line1, const char *line2);
```

- [ ] **Step 2: Implementar a tela de config**

Crie `twatch/src/config_view.cpp`:

```cpp
#include "config_view.h"

#include <stdio.h>

// Faixa do contador. Isolada para o tick poder apagá-la sem repintar a tela.
#define COUNTDOWN_TOP_Y 190
#define COUNTDOWN_HEIGHT 18

void config_view_draw(TTGOClass *watch, const char *apSsid, const char *apPass,
                      const char *url, uint32_t secondsLeft, bool lowBattery)
{
    TFT_eSPI *tft = watch->tft;
    tft->fillScreen(TFT_BLACK);
    tft->setTextDatum(MC_DATUM);

    tft->setTextFont(2);
    tft->setTextColor(TFT_WHITE, TFT_BLACK);
    tft->drawString("CONFIGURACAO", 120, 28);
    tft->drawFastHLine(30, 44, 180, TFT_DARKGREY);

    tft->setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft->drawString("conecte o celular na rede", 120, 64);

    tft->setTextFont(4);
    tft->setTextColor(TFT_WHITE, TFT_BLACK);
    tft->drawString(apSsid, 120, 88);

    tft->setTextFont(2);
    tft->setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft->drawString("senha", 120, 112);
    tft->setTextColor(TFT_WHITE, TFT_BLACK);
    tft->drawString(apPass, 120, 130);

    tft->setTextColor(TFT_CYAN, TFT_BLACK);
    tft->drawString(url, 120, 154);

    if (lowBattery) {
        // Avisa, mas não bloqueia: config é justamente o que se precisa quando
        // algo quebrou. O painel aborta com bateria baixa; aqui não.
        tft->setTextColor(TFT_ORANGE, TFT_BLACK);
        tft->drawString("bateria baixa", 120, 174);
    }

    config_view_tick(watch, secondsLeft);

    tft->setTextFont(2);
    tft->setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft->drawString("toque p/ sair", 120, 220);
}

void config_view_tick(TTGOClass *watch, uint32_t secondsLeft)
{
    TFT_eSPI *tft = watch->tft;

    char buf[24];
    snprintf(buf, sizeof(buf), "fecha em %u:%02u", (unsigned) (secondsLeft / 60),
             (unsigned) (secondsLeft % 60));

    // Apaga a faixa antes de escrever: drawString com cor de fundo só cobre a
    // caixa do texto novo, e o texto encolhe conforme o contador cai.
    tft->fillRect(0, COUNTDOWN_TOP_Y, 240, COUNTDOWN_HEIGHT, TFT_BLACK);
    tft->setTextDatum(MC_DATUM);
    tft->setTextFont(2);
    tft->setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft->drawString(buf, 120, COUNTDOWN_TOP_Y + COUNTDOWN_HEIGHT / 2);
}

void config_view_message(TTGOClass *watch, const char *line1, const char *line2)
{
    TFT_eSPI *tft = watch->tft;
    tft->fillScreen(TFT_BLACK);
    tft->setTextDatum(MC_DATUM);

    tft->setTextFont(4);
    tft->setTextColor(TFT_WHITE, TFT_BLACK);
    tft->drawString(line1, 120, 105);

    tft->setTextFont(2);
    tft->setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft->drawString(line2, 120, 140);
}
```

- [ ] **Step 3: Desenhar a engrenagem no relógio**

Substitua o conteúdo de `twatch/src/clock_view.cpp`:

```cpp
#include "clock_view.h"

#include <math.h>

#include "ui_layout.h"

// Engrenagem simples: disco com furo e seis dentes. As fontes do TFT_eSPI não
// têm glifo de engrenagem, então primitivas é o caminho.
static void drawGear(TFT_eSPI *tft, int16_t cx, int16_t cy, uint16_t color)
{
    for (int i = 0; i < 6; i++) {
        const float a = (float) i * 3.14159265f / 3.0f;
        const int16_t tx = cx + (int16_t) (cosf(a) * 11.0f);
        const int16_t ty = cy + (int16_t) (sinf(a) * 11.0f);
        tft->fillCircle(tx, ty, 3, color);
    }
    tft->fillCircle(cx, cy, 9, color);
    tft->fillCircle(cx, cy, 4, TFT_BLACK);
}

void clock_view_draw(TTGOClass *watch, bool timeIsValid)
{
    TFT_eSPI *tft = watch->tft;
    tft->fillScreen(TFT_BLACK);
    tft->setTextDatum(MC_DATUM);

    // Desenhado ANTES dos dois ramos abaixo: o ramo de hora inválida termina em
    // return, e a engrenagem precisa existir justamente ali — é a tela em que o
    // usuário ainda não configurou nada.
    tft->drawFastHLine(40, CONFIG_STRIP_TOP_Y, 160, TFT_DARKGREY);
    drawGear(tft, 120, 214, TFT_DARKGREY);

    if (!timeIsValid) {
        // O PCF8563 não descobre a hora sozinho, e a única janela de rede
        // é o painel. Até visitá-lo uma vez, não há hora para mostrar.
        tft->setTextColor(TFT_DARKGREY, TFT_BLACK);
        tft->setTextFont(6);
        tft->drawString("--:--", 120, 100);
        tft->setTextFont(2);
        tft->drawString("abra o painel", 120, 148);
        tft->drawString("para acertar a hora", 120, 166);
        return;
    }

    RTC_Date now = watch->rtc->getDateTime();

    char buf[8];
    snprintf(buf, sizeof(buf), "%02d:%02d", now.hour, now.minute);
    tft->setTextColor(TFT_WHITE, TFT_BLACK);
    tft->setTextFont(7);
    tft->drawString(buf, 120, 100);

    snprintf(buf, sizeof(buf), "%02d/%02d", now.day, now.month);
    tft->setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft->setTextFont(2);
    tft->drawString(buf, 120, 150);
}
```

O relógio e a data subiram 10 px para não encostar na faixa nova.

- [ ] **Step 4: Ligar o botão em main.cpp**

Acrescente aos includes de `twatch/src/main.cpp`:

```cpp
#include "config_view.h"
#include "ui_layout.h"
```

Declare, junto dos outros estáticos do topo:

```cpp
// Mostrados na tela de config; viram os do portal na Task 7.
static const char *AP_SSID = "T-Watch-Setup";
static const char *AP_PASS = "bitaxe1234";
static const char *AP_URL = "http://192.168.4.1";

static uint32_t lastCountdown = 0;
```

Acrescente a função de entrada em config, logo depois de `enterPanel()`:

```cpp
static void enterConfig()
{
    const int battery = watch->power->getBattPercentage();
    const bool lowBattery = (battery >= 0 && battery < (int) g_settings.batteryMinPct);

    lastCountdown = IDLE_TIMEOUT_CONFIG_MS / 1000;
    config_view_draw(watch, AP_SSID, AP_PASS, AP_URL, lastCountdown, lowBattery);
}
```

Substitua o corpo de `loop()` por:

```cpp
void loop()
{
    int16_t tx = 0, ty = 0;
    const bool touched = watch->getTouch(tx, ty);

    if (touched) {
        // A posição precisa ser copiada ANTES de drenar o toque: o laço abaixo
        // chama getTouch de novo e sobrescreve tx/ty, e a última leitura (a que
        // falha) não tem coordenada válida.
        const int16_t px = tx;
        const int16_t py = ty;

        int16_t dx = 0, dy = 0;
        while (watch->getTouch(dx, dy)) {
            delay(20);
        }

        const AppEvent ev = (state == STATE_RELOGIO && clock_hit(px, py) == HIT_CONFIG)
                                ? EVENT_TOUCH_CONFIG
                                : EVENT_TOUCH;
        state = state_next(state, ev, 0, idleTimeouts());

        if (state == STATE_PAINEL) {
            enterPanel();
        } else if (state == STATE_CONFIG) {
            enterConfig();
        } else {
            clock_view_draw(watch, g_timeIsValid);
        }
        lastInteraction = millis();
        return;
    }

    if (state == STATE_CONFIG) {
        const uint32_t elapsed = millis() - lastInteraction;
        const uint32_t left =
            elapsed >= IDLE_TIMEOUT_CONFIG_MS ? 0 : (IDLE_TIMEOUT_CONFIG_MS - elapsed) / 1000;
        if (left != lastCountdown) {
            lastCountdown = left;
            config_view_tick(watch, left);
        }
    }

    state = state_next(state, EVENT_TICK, millis() - lastInteraction, idleTimeouts());
    if (state == STATE_DEEP_SLEEP) {
        goToSleep();
    }

    // O modo de config precisa de laço curto: na Task 7 é aqui que o servidor
    // web é atendido.
    delay(state == STATE_CONFIG ? 5 : 50);
}
```

- [ ] **Step 5: Compilar, gravar e verificar no aparelho**

Run: `cd twatch && pio run -t upload --upload-port COM5`
Expected, no relógio:
1. A engrenagem aparece no rodapé da tela do relógio.
2. Tocar no meio da tela abre o painel, como sempre.
3. Tocar na engrenagem abre a tela de config, com o contador caindo de 3:00.
4. Tocar em qualquer ponto da tela de config volta ao relógio.
5. Deixando parado na config, a tela apaga aos 3 minutos.

- [ ] **Step 6: Commit**

```bash
git add twatch/src/config_view.h twatch/src/config_view.cpp twatch/src/clock_view.cpp twatch/src/main.cpp
git commit -m "feat: add config button and config screen"
```

---

### Task 7: Portal cativo

**Files:**
- Create: `twatch/src/config_portal.h`, `twatch/src/config_portal.cpp`
- Modify: `twatch/src/main.cpp`

**Interfaces:**
- Consumes: `Settings`, `SettingsForm`, `settings_apply_form()`, `settings_error_message()` (Task 1); `settings_save()` (Task 3); `config_view_message()` (Task 6); `snapshot_clear()` (Task 5).
- Produces: `bool config_portal_begin(const Settings &current)`, `bool config_portal_poll()`, `bool config_portal_should_restart()`, `void config_portal_end()`, e as constantes `CONFIG_AP_SSID`, `CONFIG_AP_PASS`, `CONFIG_AP_URL`.

- [ ] **Step 1: Escrever o header**

Crie `twatch/src/config_portal.h`:

```cpp
#pragma once

#include "settings.h"

// Mostrados na tela do relógio para o usuário digitar no celular.
extern const char *CONFIG_AP_SSID;
extern const char *CONFIG_AP_PASS;
extern const char *CONFIG_AP_URL;

// Sobe SoftAP, DNS cativo e servidor web. false se o AP não subir.
bool config_portal_begin(const Settings &current);

// Atende uma rodada de DNS e HTTP. Devolve true se alguma requisição foi
// servida — o chamador usa isso para rearmar o contador de ociosidade, senão o
// relógio dorme no meio da digitação no celular.
bool config_portal_poll();

// true depois de uma gravação bem-sucedida: o chamador deve reiniciar.
bool config_portal_should_restart();

// Derruba servidor, DNS e AP. Seguro chamar sem ter começado.
void config_portal_end();
```

- [ ] **Step 2: Implementar o portal**

Crie `twatch/src/config_portal.cpp`:

```cpp
#include "config_portal.h"

#include <DNSServer.h>
#include <WebServer.h>
#include <WiFi.h>

#include "settings_store.h"

const char *CONFIG_AP_SSID = "T-Watch-Setup";
const char *CONFIG_AP_PASS = "bitaxe1234";  // WPA2 exige 8 caracteres ou mais
const char *CONFIG_AP_URL = "http://192.168.4.1";

static DNSServer *dns = nullptr;
static WebServer *server = nullptr;
static Settings g_current;
static bool g_served = false;
static bool g_restart = false;

// O SSID da rede do usuário pode conter aspas e &, e ele volta para dentro de
// um atributo HTML. Sem escapar, um SSID com aspa quebra o formulário.
static String htmlEscape(const char *s)
{
    String out;
    for (const char *p = s; *p != '\0'; p++) {
        switch (*p) {
        case '&': out += "&amp;"; break;
        case '<': out += "&lt;"; break;
        case '>': out += "&gt;"; break;
        case '"': out += "&quot;"; break;
        case '\'': out += "&#39;"; break;
        default: out += *p; break;
        }
    }
    return out;
}

static String tzOptions(int16_t selected)
{
    String out;
    for (int m = SETTINGS_TZ_MIN_MINUTES; m <= SETTINGS_TZ_MAX_MINUTES; m += SETTINGS_TZ_STEP_MINUTES) {
        const int magnitude = m < 0 ? -m : m;
        char label[8];
        snprintf(label, sizeof(label), "%c%02d:%02d", m < 0 ? '-' : '+', magnitude / 60,
                 magnitude % 60);

        out += "<option value=";
        out += m;
        if (m == selected) {
            out += " selected";
        }
        out += ">UTC";
        out += label;
        out += "</option>";
    }
    return out;
}

static String numberField(const char *label, const char *name, uint32_t value)
{
    String out = "<label>";
    out += label;
    out += "<input name=";
    out += name;
    out += " type=number value=";
    out += value;
    out += "></label>";
    return out;
}

static void sendForm(const char *errorMsg)
{
    String h = F("<!doctype html><html lang=pt-br><meta charset=utf-8>"
                 "<meta name=viewport content='width=device-width,initial-scale=1'>"
                 "<title>T-Watch</title><style>"
                 "body{font-family:sans-serif;margin:16px;max-width:420px}"
                 "label{display:block;margin-top:12px;font-size:14px;color:#333}"
                 "input,select{width:100%;padding:8px;font-size:16px;box-sizing:border-box}"
                 "button{margin-top:20px;padding:12px;width:100%;font-size:16px}"
                 ".e{background:#fdd;border:1px solid #c00;padding:8px}"
                 "</style><h2>T-Watch</h2>");

    if (errorMsg != nullptr) {
        h += "<p class=e>";
        h += htmlEscape(errorMsg);
        h += "</p>";
    }

    h += "<form method=post action=/save>";

    h += "<label>Rede WiFi<input name=ssid maxlength=32 value='";
    h += htmlEscape(g_current.wifiSsid);
    h += "'></label>";

    // A senha gravada NUNCA volta para a página: este AP está ao alcance de
    // qualquer vizinho. Campo vazio significa "manter a atual".
    h += F("<label>Senha WiFi<input name=pass type=password maxlength=63 "
           "placeholder='(inalterada)'></label>");

    h += "<label>IP do Bitaxe<input name=host maxlength=15 value='";
    h += htmlEscape(g_current.bitaxeHost);
    h += "'></label>";

    h += "<label>Fuso horario<select name=tz>";
    h += tzOptions(g_current.tzMinutes);
    h += "</select></label>";

    h += numberField("Timeout de WiFi (ms)", "wifims", g_current.wifiTimeoutMs);
    h += numberField("Timeout de HTTP (ms)", "httpms", g_current.httpTimeoutMs);
    h += numberField("Bateria minima (%)", "battmin", g_current.batteryMinPct);
    h += numberField("Ociosidade do relogio (ms)", "idlerel", g_current.idleRelogioMs);
    h += numberField("Ociosidade do painel (ms)", "idlepan", g_current.idlePainelMs);

    h += F("<button type=submit>Salvar e reiniciar</button></form>");

    server->send(200, "text/html; charset=utf-8", h);
}

static void handleRoot()
{
    g_served = true;
    sendForm(nullptr);
}

static void handleSave()
{
    g_served = true;

    // As Strings precisam viver até o fim da função: SettingsForm guarda
    // ponteiros para dentro delas.
    const String ssid = server->arg("ssid");
    const String pass = server->arg("pass");
    const String host = server->arg("host");
    const String tz = server->arg("tz");
    const String wifims = server->arg("wifims");
    const String httpms = server->arg("httpms");
    const String battmin = server->arg("battmin");
    const String idlerel = server->arg("idlerel");
    const String idlepan = server->arg("idlepan");

    SettingsForm f;
    f.ssid = ssid.c_str();
    f.pass = pass.c_str();
    f.host = host.c_str();
    f.tzMinutes = tz.c_str();
    f.wifiTimeoutMs = wifims.c_str();
    f.httpTimeoutMs = httpms.c_str();
    f.batteryMinPct = battmin.c_str();
    f.idleRelogioMs = idlerel.c_str();
    f.idlePainelMs = idlepan.c_str();

    Settings candidate = {};
    const SettingsError e = settings_apply_form(g_current, f, candidate);
    if (e != SETTINGS_OK) {
        sendForm(settings_error_message(e));
        return;
    }

    if (!settings_save(candidate)) {
        // Não reinicia: o estado anterior continua válido e utilizável.
        sendForm("Falha ao gravar na memoria do relogio. Tente de novo.");
        return;
    }

    g_current = candidate;
    g_restart = true;
    server->send(200, "text/html; charset=utf-8",
                 F("<!doctype html><html lang=pt-br><meta charset=utf-8>"
                   "<meta name=viewport content='width=device-width,initial-scale=1'>"
                   "<h2>Salvo</h2><p>O relogio esta reiniciando com a configuracao nova.</p>"));
}

static void handleNotFound()
{
    g_served = true;
    // Redirecionar tudo para a raiz é o que faz o celular reconhecer a rede
    // como portal cativo e abrir a página sozinho.
    server->sendHeader("Location", CONFIG_AP_URL, true);
    server->send(302, "text/plain", "");
}

bool config_portal_begin(const Settings &current)
{
    g_current = current;
    g_restart = false;
    g_served = false;

    WiFi.mode(WIFI_AP);
    if (!WiFi.softAP(CONFIG_AP_SSID, CONFIG_AP_PASS)) {
        WiFi.mode(WIFI_OFF);
        return false;
    }

    dns = new DNSServer();
    server = new WebServer(80);

    dns->start(53, "*", WiFi.softAPIP());

    server->on("/", HTTP_GET, handleRoot);
    server->on("/save", HTTP_POST, handleSave);
    server->onNotFound(handleNotFound);
    server->begin();

    return true;
}

bool config_portal_poll()
{
    if (server == nullptr) {
        return false;
    }
    g_served = false;
    dns->processNextRequest();
    server->handleClient();
    return g_served;
}

bool config_portal_should_restart()
{
    return g_restart;
}

void config_portal_end()
{
    // Saída antecipada de verdade: sem isso, chamar esta função no caminho
    // relógio -> painel desligaria o rádio logo antes de o painel precisar
    // dele.
    if (server == nullptr) {
        return;
    }

    server->stop();
    delete server;
    server = nullptr;
    if (dns != nullptr) {
        dns->stop();
        delete dns;
        dns = nullptr;
    }
    g_restart = false;
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
}
```

- [ ] **Step 3: Ligar o portal em main.cpp**

Acrescente aos includes:

```cpp
#include "config_portal.h"
```

Remova as três constantes provisórias `AP_SSID`, `AP_PASS` e `AP_URL`
declaradas na Task 6 — agora elas vêm de `config_portal.h`.

Substitua `enterConfig()` por:

```cpp
static void enterConfig()
{
    if (!config_portal_begin(g_settings)) {
        config_view_message(watch, "falha ao subir", "o ponto de acesso");
        delay(2000);
        state = STATE_RELOGIO;
        clock_view_draw(watch, g_timeIsValid);
        return;
    }

    const int battery = watch->power->getBattPercentage();
    const bool lowBattery = (battery >= 0 && battery < (int) g_settings.batteryMinPct);

    lastCountdown = IDLE_TIMEOUT_CONFIG_MS / 1000;
    config_view_draw(watch, CONFIG_AP_SSID, CONFIG_AP_PASS, CONFIG_AP_URL, lastCountdown,
                     lowBattery);
}
```

Substitua o bloco `if (state == STATE_CONFIG)` de `loop()` por:

```cpp
    if (state == STATE_CONFIG) {
        // Requisição atendida conta como interação: sem isso o relógio dorme
        // no meio da digitação, já que o usuário não está tocando nele.
        if (config_portal_poll()) {
            lastInteraction = millis();
        }

        if (config_portal_should_restart()) {
            config_view_message(watch, "salvo", "reiniciando");
            // O cache e a hora ficaram obsoletos se o IP ou o fuso mudaram, e a
            // RTC memory sobrevive a reset por software — limpar é obrigatório.
            snapshot_clear();
            g_timeIsValid = false;
            delay(1000);  // dá tempo de a resposta HTTP sair pela rede
            config_portal_end();
            ESP.restart();
        }

        const uint32_t elapsed = millis() - lastInteraction;
        const uint32_t left =
            elapsed >= IDLE_TIMEOUT_CONFIG_MS ? 0 : (IDLE_TIMEOUT_CONFIG_MS - elapsed) / 1000;
        if (left != lastCountdown) {
            lastCountdown = left;
            config_view_tick(watch, left);
        }
    }
```

No bloco de toque, derrube o portal ao sair da config. Logo depois da chamada de
`state_next`, insira:

```cpp
        if (state != STATE_CONFIG) {
            // No-op se o portal não estiver no ar.
            config_portal_end();
        }
```

E na primeira linha de `goToSleep()`, antes de `watch->closeBL()`:

```cpp
    // Dormir com o AP no ar deixaria o rádio ligado no estado mais caro.
    config_portal_end();
```

- [ ] **Step 4: Compilar e gravar**

Run: `cd twatch && pio run -t upload --upload-port COM5`
Expected: SUCCESS.

- [ ] **Step 5: Verificar o caminho completo no aparelho**

Com o relógio na mão e um celular:
1. Tocar na engrenagem abre a tela de config e `T-Watch-Setup` aparece na lista de redes do celular.
2. Conectar com a senha `bitaxe1234` faz o celular abrir o portal sozinho.
3. O formulário chega preenchido, e o campo de senha está **vazio**.
4. Salvar com `bitaxe.local` no campo de IP devolve a mensagem de erro e não regrava.
5. Salvar com `4000abc` no timeout de HTTP devolve erro.
6. Salvar válido mostra "Salvo" no celular, "salvo/reiniciando" no relógio, e o aparelho reinicia.
7. Depois do reinício, abrir o painel conecta na rede configurada e busca o Bitaxe.
8. Digitando no celular por mais de 3 minutos sem tocar no relógio, ele **não** dorme.
9. Sem tocar em nada e sem carregar a página, a tela dorme aos 3 minutos.

- [ ] **Step 6: Commit**

```bash
git add twatch/src/config_portal.h twatch/src/config_portal.cpp twatch/src/main.cpp
git commit -m "feat: add captive portal for runtime configuration"
```

---

### Task 8: Documentação

**Files:**
- Modify: `twatch/README.md`

**Interfaces:**
- Consumes: tudo das tarefas anteriores.
- Produces: nada em código.

- [ ] **Step 1: Reescrever a seção "Configuração" do README**

Em `twatch/README.md`, substitua a seção `## Configuração` inteira por:

```markdown
## Configuração

Há dois caminhos, e o segundo é o normal no dia a dia.

**Semente de compilação.** `include/config.h.example` é o único versionado — só
placeholders. `include/config.h` é a sua cópia local com os valores verdadeiros;
está no `.gitignore` e nunca deve ser commitado. Esses valores são usados
**apenas quando o NVS ainda está vazio**, no primeiro boot depois de apagar a
flash.

**Portal cativo.** Toque na engrenagem no rodapé da tela do relógio. O aparelho
sobe um ponto de acesso:

| Item | Valor |
|---|---|
| Rede | `T-Watch-Setup` |
| Senha | `bitaxe1234` |
| Endereço | `http://192.168.4.1` |

Conecte o celular e o portal abre sozinho. Dá para configurar rede WiFi, IP do
Bitaxe, fuso horário, timeouts de WiFi e HTTP, bateria mínima e os tempos de
ociosidade antes do deep sleep. Salvar grava no NVS e reinicia o relógio.

O ponto de acesso vive no máximo **3 minutos** — é uma trava de bateria. Carregar
ou enviar a página rearma esse relógio, então digitar sem pressa não derruba o
portal.

O campo de senha do WiFi sempre chega vazio: a senha gravada nunca é devolvida
para a página, porque o ponto de acesso está ao alcance de qualquer vizinho.
Deixe vazio para manter a senha atual.

O IP do Bitaxe precisa ser IPv4 — `bitaxe.local` é recusado pelo formulário, e
com razão: o mDNS resolve para IPv6 e a API devolve 401.
```

- [ ] **Step 2: Atualizar a seção "Estado atual"**

Na seção `## Estado atual` do mesmo arquivo, substitua o parágrafo de abertura
("O código está escrito e revisado, mas **ainda não foi compilado nem
gravado**...") por uma frase que reflita o que foi de fato verificado no
aparelho durante as tarefas 3 a 7, incluindo o resultado real do `psramFound()`
lido no log de boot.

- [ ] **Step 3: Commit**

```bash
git add twatch/README.md
git commit -m "docs: document the config portal and runtime settings"
```

---

## Auto-revisão do plano

**Cobertura da spec:** todas as seções da spec têm tarefa correspondente —
módulos novos (1,2,3,6,7), mudanças em código existente (3,4,5,6,7), máquina de
estados (4), contador rearmado por HTTP (7), fluxo de salvar com restart e
limpeza de cache (7), validação (1), erros de infraestrutura (3,7), os três
arquivos de teste (1,2,4) e a verificação manual (6,7).

**Sem placeholders:** todo passo de código traz o código real. A única instrução
redacional é o passo 2 da Task 8, que depende de um resultado observado no
aparelho e não pode ser escrito antes.

**Consistência de tipos:** `Settings`, `SettingsForm` e `SettingsError` são
definidos na Task 1 e usados com os mesmos nomes nas 3 e 7. `IdleTimeouts` é
definido na Task 4 e consumido por `idleTimeouts()` em `main.cpp`. `clock_hit`
e `CONFIG_STRIP_TOP_Y` vêm da Task 2 e são usados na 6. `snapshot_clear`
vem da Task 5 e é usado na 7. `config_view_*` vem da 6 e é usado na 7.
