# T-Watch Fase 3 — os recursos parados do relógio

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Colocar para trabalhar quatro recursos do T-Watch que estavam ociosos — motor de vibração, brilho PWM, contador de passos do BMA423 e despertar por temporizador — sem comprometer a autonomia.

**Architecture:** A decisão de alertar e a de rodar a vigia nascem puras em `lib/bitaxe_core/watchdog.*`. O despertar por timer é uma fonte independente que convive com o `ext0` do toque e o `ext1` do botão PEK; ao acordar por ele, o firmware faz um ciclo silencioso — sem acender a tela — e volta a dormir.

**Tech Stack:** PlatformIO 6.1.19, `espressif32@6.9.0`, `TTGO_TWatch_Library` 1.4.2, Unity.

**Depende de:** Fases 1 e 2 concluídas.

## Global Constraints

- Alvo: TTGO T-Watch 2020 V1, PMU **AXP202**. Motor no GPIO4, BMA423 no I2C de sensores.
- Textos de UI em pt-br **sem acentos**.
- Estilo C: 4 espaços, 132 colunas, chaves de função em linha própria.
- Testes rodam **no alvo** em `COM5`. Lógica testável só em `lib/`.
- Nenhuma constante `TFT_*` fora de `src/theme.h`.
- A vigia periódica **nasce desligada** (`WATCH_INTERVAL_MIN 0`): é o único recurso que gasta bateria sem o usuário pedir.
- `git commit` ao fim de cada tarefa, com `Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>`.

---

### Task 1: Decisões da vigia (puras, TDD)

**Files:**
- Create: `twatch/lib/bitaxe_core/watchdog.h`
- Create: `twatch/lib/bitaxe_core/watchdog.cpp`
- Test: `twatch/test/test_watchdog/test_watchdog.cpp`

**Interfaces:**
- Consumes: `BATTERY_UNKNOWN` de `battery.h`, `METRICS_MIN_HASHRATE_GHS` de `metrics.h`.
- Produces: `WATCHDOG_MAX_DEVICES` (2), `struct WatchdogInputs { uint8_t deviceCount; bool online[2]; float hashRate[2]; }`, `bool watchdog_should_alert(const WatchdogInputs &in)`, `bool watchdog_should_run(uint32_t intervalMinutes, int batteryPercent, uint8_t minPct)`.

- [ ] **Step 1: Escrever o header**

`twatch/lib/bitaxe_core/watchdog.h`:

```c
#pragma once

#include <stdint.h>

#define WATCHDOG_MAX_DEVICES 2

struct WatchdogInputs
{
    uint8_t deviceCount;
    bool online[WATCHDOG_MAX_DEVICES];      // respondeu nesta rodada
    float hashRate[WATCHDOG_MAX_DEVICES];   // Gh/s; irrelevante quando offline
};

// Verdadeiro quando algum dispositivo configurado nao respondeu ou parou de
// minerar. E o gatilho da vibracao — silencio significa "esta tudo bem".
bool watchdog_should_alert(const WatchdogInputs &in);

// Verdadeiro quando a vigia periodica deve ser armada antes de dormir.
bool watchdog_should_run(uint32_t intervalMinutes, int batteryPercent, uint8_t minPct);
```

- [ ] **Step 2: Escrever a suíte**

`twatch/test/test_watchdog/test_watchdog.cpp`:

```c
#include <Arduino.h>
#include <unity.h>

#include "battery.h"
#include "watchdog.h"

// Dois aparelhos saudaveis, minerando. Cada teste estraga UM aspecto.
static WatchdogInputs saudavel(void)
{
    WatchdogInputs in = {};
    in.deviceCount = 2;
    in.online[0] = true;
    in.online[1] = true;
    in.hashRate[0] = 1362.0f;
    in.hashRate[1] = 1288.0f;
    return in;
}

void test_tudo_bem_nao_alerta(void)
{
    TEST_ASSERT_FALSE(watchdog_should_alert(saudavel()));
}

void test_dispositivo_offline_alerta(void)
{
    WatchdogInputs in = saudavel();
    in.online[1] = false;
    TEST_ASSERT_TRUE(watchdog_should_alert(in));
}

void test_hashrate_zerado_alerta(void)
{
    // Responde, mas parou de minerar: e falha tanto quanto nao responder.
    WatchdogInputs in = saudavel();
    in.hashRate[0] = 0.0f;
    TEST_ASSERT_TRUE(watchdog_should_alert(in));
}

void test_segundo_dispositivo_nao_configurado_e_ignorado(void)
{
    // Com deviceCount 1, o lixo do slot 1 nao pode disparar alerta.
    WatchdogInputs in = saudavel();
    in.deviceCount = 1;
    in.online[1] = false;
    in.hashRate[1] = 0.0f;
    TEST_ASSERT_FALSE(watchdog_should_alert(in));
}

void test_contagem_invalida_nao_alerta(void)
{
    WatchdogInputs in = saudavel();
    in.deviceCount = 0;
    TEST_ASSERT_FALSE(watchdog_should_alert(in));
}

void test_intervalo_zero_nao_roda(void)
{
    // Zero e o padrao de fabrica: a vigia so existe se o usuario pedir.
    TEST_ASSERT_FALSE(watchdog_should_run(0, 90, 15));
}

void test_bateria_boa_roda(void)
{
    TEST_ASSERT_TRUE(watchdog_should_run(15, 90, 15));
}

void test_bateria_abaixo_do_minimo_nao_roda(void)
{
    TEST_ASSERT_FALSE(watchdog_should_run(15, 9, 15));
}

void test_bateria_exatamente_no_minimo_roda(void)
{
    TEST_ASSERT_TRUE(watchdog_should_run(15, 15, 15));
}

void test_bateria_desconhecida_nao_bloqueia(void)
{
    // Mesma regra que o painel ja aplica: so bloqueia o que se sabe estar baixo.
    TEST_ASSERT_TRUE(watchdog_should_run(15, BATTERY_UNKNOWN, 15));
}

void setUp(void) {}
void tearDown(void) {}

void setup()
{
    delay(2000);
    UNITY_BEGIN();
    RUN_TEST(test_tudo_bem_nao_alerta);
    RUN_TEST(test_dispositivo_offline_alerta);
    RUN_TEST(test_hashrate_zerado_alerta);
    RUN_TEST(test_segundo_dispositivo_nao_configurado_e_ignorado);
    RUN_TEST(test_contagem_invalida_nao_alerta);
    RUN_TEST(test_intervalo_zero_nao_roda);
    RUN_TEST(test_bateria_boa_roda);
    RUN_TEST(test_bateria_abaixo_do_minimo_nao_roda);
    RUN_TEST(test_bateria_exatamente_no_minimo_roda);
    RUN_TEST(test_bateria_desconhecida_nao_bloqueia);
    UNITY_END();
}

void loop() {}
```

- [ ] **Step 3: Rodar e ver falhar**

```bash
cd twatch
pio test --upload-port COM5 -f test_watchdog
```

Esperado: falha de compilação — `watchdog.cpp` não existe.

- [ ] **Step 4: Implementar**

`twatch/lib/bitaxe_core/watchdog.cpp`:

```c
#include "watchdog.h"

#include "battery.h"
#include "metrics.h"

bool watchdog_should_alert(const WatchdogInputs &in)
{
    if (in.deviceCount < 1 || in.deviceCount > WATCHDOG_MAX_DEVICES) {
        return false;
    }

    for (uint8_t i = 0; i < in.deviceCount; i++) {
        if (!in.online[i]) {
            return true;
        }
        // Mesma constante que o painel usa para decidir que o J/TH nao
        // significa nada: abaixo dela o aparelho nao esta minerando.
        if (in.hashRate[i] < METRICS_MIN_HASHRATE_GHS) {
            return true;
        }
    }
    return false;
}

bool watchdog_should_run(uint32_t intervalMinutes, int batteryPercent, uint8_t minPct)
{
    if (intervalMinutes == 0) {
        return false;
    }
    // Desconhecida nao bloqueia: senao um AXP202 com registrador ruim
    // desligaria a vigia para sempre, sem sintoma.
    if (batteryPercent == BATTERY_UNKNOWN) {
        return true;
    }
    return batteryPercent >= (int) minPct;
}
```

- [ ] **Step 5: Rodar e ver passar**

```bash
cd twatch
pio test --upload-port COM5 -f test_watchdog
```

Esperado: 10 testes, todos PASS.

- [ ] **Step 6: Commit**

```bash
git add twatch/lib/bitaxe_core/watchdog.h twatch/lib/bitaxe_core/watchdog.cpp twatch/test/test_watchdog/
git commit -m "feat: add watchdog alert and scheduling decisions"
```

---

### Task 2: Configuração de brilho e intervalo da vigia

**Files:**
- Modify: `twatch/lib/bitaxe_core/settings.h`
- Modify: `twatch/lib/bitaxe_core/settings.cpp`
- Modify: `twatch/src/settings_store.cpp`
- Modify: `twatch/src/config_portal.cpp`
- Modify: `twatch/include/config.h.example`
- **Não tocar:** `twatch/include/config.h` (git-ignored, credenciais reais — a semente vai por guarda `#ifndef` no `settings_store.cpp`)
- Test: `twatch/test/test_settings/test_settings.cpp`

**Interfaces:**
- Produces: campos `uint8_t brightnessClock`, `uint8_t brightnessPanel`, `uint16_t watchIntervalMin` em `Settings`; campos `const char *brightnessClock`, `*brightnessPanel`, `*watchIntervalMin` em `SettingsForm`; erros `SETTINGS_ERR_BRIGHTNESS` e `SETTINGS_ERR_WATCH`.

- [ ] **Step 1: Escrever os testes novos**

Em `twatch/test/test_settings/test_settings.cpp`, acrescentar ao final de `baseline()`, antes do `return`:

```c
    s.brightnessClock = 40;
    s.brightnessPanel = 100;
    s.watchIntervalMin = 0;
```

E ao final de `baselineForm()`:

```c
    f.brightnessClock = "40";
    f.brightnessPanel = "100";
    f.watchIntervalMin = "0";
```

Depois os casos:

```c
void test_brilho_abaixo_do_minimo_reprova(void)
{
    // Abaixo de 10% a tela fica ilegivel no sol — e o usuario nao teria como
    // voltar atras sem enxergar o portal.
    Settings s = baseline();
    s.brightnessClock = 5;
    TEST_ASSERT_EQUAL(SETTINGS_ERR_BRIGHTNESS, settings_validate(s));
}

void test_brilho_do_painel_acima_de_cem_reprova(void)
{
    Settings s = baseline();
    s.brightnessPanel = 120;
    TEST_ASSERT_EQUAL(SETTINGS_ERR_BRIGHTNESS, settings_validate(s));
}

void test_vigia_desligada_aprova(void)
{
    Settings s = baseline();
    s.watchIntervalMin = 0;
    TEST_ASSERT_EQUAL(SETTINGS_OK, settings_validate(s));
}

void test_vigia_abaixo_do_minimo_reprova(void)
{
    // Menos de 5 min drenaria a bateria em uma tarde.
    Settings s = baseline();
    s.watchIntervalMin = 3;
    TEST_ASSERT_EQUAL(SETTINGS_ERR_WATCH, settings_validate(s));
}

void test_vigia_no_minimo_aprova(void)
{
    Settings s = baseline();
    s.watchIntervalMin = SETTINGS_WATCH_MIN_MINUTES;
    TEST_ASSERT_EQUAL(SETTINGS_OK, settings_validate(s));
}

void test_vigia_acima_do_teto_reprova(void)
{
    Settings s = baseline();
    s.watchIntervalMin = SETTINGS_WATCH_MAX_MINUTES + 1;
    TEST_ASSERT_EQUAL(SETTINGS_ERR_WATCH, settings_validate(s));
}

void test_form_vigia_com_lixo_reprova(void)
{
    SettingsForm f = baselineForm();
    f.watchIntervalMin = "15min";

    Settings out = {};
    TEST_ASSERT_EQUAL(SETTINGS_ERR_WATCH, settings_apply_form(baseline(), f, out));
}

void test_form_brilho_valido_grava(void)
{
    SettingsForm f = baselineForm();
    f.brightnessClock = "25";

    Settings out = {};
    TEST_ASSERT_EQUAL(SETTINGS_OK, settings_apply_form(baseline(), f, out));
    TEST_ASSERT_EQUAL(25, out.brightnessClock);
}
```

Registrar os oito no `setup()`.

- [ ] **Step 2: Rodar e ver falhar**

```bash
cd twatch
pio test --upload-port COM5 -f test_settings
```

Esperado: falha de compilação — os campos e erros novos não existem.

- [ ] **Step 3: Estender o header**

Em `twatch/lib/bitaxe_core/settings.h`, junto dos outros limites:

```c
#define SETTINGS_BRIGHTNESS_MIN_PCT 10
#define SETTINGS_BRIGHTNESS_MAX_PCT 100

// Zero desliga a vigia. O piso de 5 min existe para impedir uma configuracao
// que drene o aparelho em uma tarde.
#define SETTINGS_WATCH_MIN_MINUTES 5
#define SETTINGS_WATCH_MAX_MINUTES 240
```

No `struct Settings`, ao final:

```c
    uint8_t brightnessClock;
    uint8_t brightnessPanel;
    uint16_t watchIntervalMin;  // 0 = vigia desligada
```

No `struct SettingsForm`, ao final:

```c
    const char *brightnessClock;
    const char *brightnessPanel;
    const char *watchIntervalMin;
```

No enum `SettingsError`, ao final:

```c
    SETTINGS_ERR_BRIGHTNESS,
    SETTINGS_ERR_WATCH,
```

- [ ] **Step 4: Implementar validação e formulário**

Em `settings_validate`, antes do `return SETTINGS_OK`:

```c
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
```

Em `settings_error_message`, ao final dos casos:

```c
    case SETTINGS_ERR_BRIGHTNESS: return "Brilho: 10 a 100 por cento.";
    case SETTINGS_ERR_WATCH: return "Vigia: 0 para desligar, ou de 5 a 240 minutos.";
```

Em `settings_apply_form`, antes do `return settings_validate(out)`:

```c
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
```

Em `settings_equal`, acrescentar ao encadeamento:

```c
           a.brightnessClock == b.brightnessClock && a.brightnessPanel == b.brightnessPanel &&
           a.watchIntervalMin == b.watchIntervalMin &&
```

- [ ] **Step 5: Semear, persistir e expor no portal**

Em `twatch/include/config.h.example`, **apenas**:

```c
// Brilho por tela, em porcentagem. O relogio fica mais escuro de proposito.
#define BRIGHTNESS_CLOCK 40
#define BRIGHTNESS_PANEL 100

// Vigia periodica em minutos. 0 = desligada (padrao: e o unico recurso que
// gasta bateria sem o usuario pedir).
#define WATCH_INTERVAL_MIN 0
```

**Não edite `twatch/include/config.h`.** Ele é git-ignored e guarda credenciais reais de WiFi; qualquer `config.h` já existente (o do usuário, o de outra máquina) ficaria sem esses três defines e o build quebraria com um erro obscuro. Use a mesma guarda que a fase 2 adotou para `BITAXE_HOST2` — em `twatch/src/settings_store.cpp`, antes do primeiro uso:

```c
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
```

Em `twatch/src/settings_store.cpp`, em `settings_load`:

```c
    out.brightnessClock = p.getUChar("brtclk", BRIGHTNESS_CLOCK);
    out.brightnessPanel = p.getUChar("brtpan", BRIGHTNESS_PANEL);
    out.watchIntervalMin = p.getUShort("watchmin", WATCH_INTERVAL_MIN);
```

E em `settings_save`:

```c
    p.putUChar("brtclk", s.brightnessClock);
    p.putUChar("brtpan", s.brightnessPanel);
    p.putUShort("watchmin", s.watchIntervalMin);
```

Em `twatch/src/config_portal.cpp`, em `sendForm`, depois dos campos de ociosidade:

```c
    h += numberField("Brilho do relogio (%)", "brtclk", g_current.brightnessClock);
    h += numberField("Brilho do painel (%)", "brtpan", g_current.brightnessPanel);
    h += numberField("Vigia periodica (min, 0 desliga)", "watchmin", g_current.watchIntervalMin);
```

E em `handleSave`, junto das outras Strings e ponteiros:

```c
    const String brtclk = server->arg("brtclk");
    const String brtpan = server->arg("brtpan");
    const String watchmin = server->arg("watchmin");
```

```c
    f.brightnessClock = brtclk.c_str();
    f.brightnessPanel = brtpan.c_str();
    f.watchIntervalMin = watchmin.c_str();
```

- [ ] **Step 6: Rodar e ver passar**

```bash
cd twatch
pio test --upload-port COM5 -f test_settings
```

Esperado: 36 testes (28 da Fase 2 + 8 novos), todos PASS.

- [ ] **Step 7: Commit**

```bash
git add twatch/lib/bitaxe_core/settings.h twatch/lib/bitaxe_core/settings.cpp \
        twatch/src/settings_store.cpp twatch/src/config_portal.cpp \
        twatch/include/config.h.example twatch/test/test_settings/test_settings.cpp
git commit -m "feat: add per-screen brightness and watchdog interval settings"
```

---

### Task 3: Vibração de alerta e brilho por tela

**Files:**
- Modify: `twatch/src/main.cpp`

**Interfaces:**
- Consumes: `watchdog_should_alert`, `screen_brightness`, `watch->motor_begin()`, `watch->shake()`.
- Produces: `static void alertBuzz(uint8_t times)`, `static void pollDevices(WatchdogInputs &health)` — a Task 5 consome esta última. `batteryNow()` **já existe** desde o fecho da Fase 1; não recrie.

- [ ] **Step 1: Ligar o motor no boot**

Em `twatch/src/main.cpp`, acrescentar aos includes:

```c
#include "watchdog.h"
```

Com esse include, `main.cpp` passa a enxergar as **três** constantes de teto de dispositivos. Existe ali um `static_assert` que hoje amarra duas; estenda-o para cobrir a terceira:

```c
static_assert(SNAPSHOT_SLOTS == PANEL_CYCLE_MAX_DEVICES && SNAPSHOT_SLOTS == WATCHDOG_MAX_DEVICES,
              "tetos de dispositivos divergiram entre cache, ciclo e vigia");
```

(A Task 1 já amarrou `WATCHDOG_MAX_DEVICES` a `PANEL_CYCLE_MAX_DEVICES` dentro de `lib/`. Esta linha fecha o triângulo no único arquivo que vê os três.)

Em `setup()`, logo depois de `watch->power->clearIRQ();`:

```c
    // O motor precisa ser inicializado antes do primeiro shake(); begin() nao
    // faz isso sozinho.
    watch->motor_begin();
```

- [ ] **Step 2: Criar o alerta tátil**

`batteryNow()` **já existe** em `main.cpp` desde o fecho da Fase 1 — é o leitor único que o relógio, o painel e a config compartilham. Não recrie nem duplique; apenas use.

Acrescentar acima de `clockModel()`:

```c
// Alerta tatil. Silencio significa "esta tudo bem", entao isto so e chamado
// quando o watchdog aponta problema.
static void alertBuzz(uint8_t times)
{
    for (uint8_t i = 0; i < times; i++) {
        if (i > 0) {
            delay(150);
        }
        watch->shake();
    }
}
```

`clockModel()` e `enterPanel()` **já chamam** `batteryNow()` desde o fecho da Fase 1. Nada a mudar neles nesta etapa — confira que continua assim e siga.

- [ ] **Step 3: Vibrar quando um Bitaxe cai**

Em `enterPanel()`, o laço de busca já alimenta `static bool g_justFetched[SNAPSHOT_SLOTS]`, criado no fecho da Fase 2 para que o dispositivo recém-buscado seja desenhado com idade zero (sem ele, uma falha de NTP pinta dado fresco de cinza com uma idade absurda). O `health.online[]` do watchdog carrega **exatamente a mesma informação**, então os dois se escrevem numa atribuição só.

**Atenção:** `g_justFetched` é estado de arquivo de propósito, e `health` é local. A alternância entre dispositivos redesenha **fora** do `enterPanel`, sem rede — se o flag voltar a ser local, o segundo dispositivo perde a marca de "recém-buscado" e volta a ser pintado como obsoleto. Não substitua `g_justFetched` por `health.online`; escreva nos dois.

A Task 5 vai precisar **deste mesmo laço** para a vigia periódica, que roda com a tela apagada. Em vez de escrevê-lo duas vezes, extraia-o agora para uma função. Duas cópias desse laço teriam de ser mantidas em sincronia em quatro pontos independentes (a disciplina de copiar o buffer estático do `fetchNote`, a gravação por slot, os dois campos de `health`) — e a Fase 2 já custou duas rodadas de correção exatamente por estado duplicado que divergiu.

Acrescente **acima de `enterPanel()`**:

```c
// Consulta todos os dispositivos configurados numa conexao JA ABERTA e
// atualiza cache, notas e o retrato de saude. Nao conecta, nao desconecta e
// nao desenha: quem chama e que manda no radio e na tela. Existe uma copia so
// desta funcao porque a vigia periodica corre exatamente o mesmo laco.
static void pollDevices(WatchdogInputs &health)
{
    const uint8_t count = settings_device_count(g_settings);
    const char *hosts[SNAPSHOT_SLOTS] = { g_settings.bitaxeHost, g_settings.bitaxeHost2 };

    health.deviceCount = count;

    // Os dois na MESMA conexao: reconectar por dispositivo custaria outro ciclo
    // de radio inteiro, que e o gasto que domina a bateria.
    for (uint8_t i = 0; i < count; i++) {
        BitaxeStatus fresh;
        int httpCode = 0;
        const FetchResult r = bitaxe_fetch(hosts[i], g_settings.httpTimeoutMs, fresh, httpCode);

        // O mesmo fato serve a dois consumidores com vidas diferentes: o
        // watchdog so precisa dele ate o fim da chamada, o desenho precisa dele
        // ate o painel fechar, porque a alternancia redesenha sem rede.
        health.online[i] = g_justFetched[i] = (r == FETCH_OK);
        health.hashRate[i] = r == FETCH_OK ? fresh.hashRate10m : 0.0f;

        if (r == FETCH_OK) {
            snapshot_save(i, fresh, nowEpoch());
            g_note[i][0] = '\0';
        } else {
            // O cache do dispositivo que falhou e preservado; so a nota muda.
            // fetchNote devolve buffer estatico: copiar AQUI, antes da proxima volta.
            snprintf(g_note[i], sizeof(g_note[i]), "%s", fetchNote(r, httpCode));
        }
    }
}
```

E em `enterPanel()`, substituir o laço de busca e a linha de desenho final por:

```c
    WatchdogInputs health = {};
    pollDevices(health);
    net_disconnect();

    if (watchdog_should_alert(health)) {
        alertBuzz(1);
    }

    const int target = g_panelDevice == PANEL_CYCLE_CLOCK ? 0 : g_panelDevice;
    drawDevice(target);
```

Se `count` e `hosts[]` ficarem sem uso no corpo de `enterPanel()` depois da extração, apague as declarações — `pollDevices` as recalcula.

Note o que **não** muda: `drawDevice` continua com um parâmetro só e continua lendo `g_justFetched[index]` por dentro — não devolva o segundo parâmetro para ela. O laço que zera `g_note` e `g_justFetched` no início de `enterPanel()` também fica onde está; você está substituindo o laço de busca e a linha de desenho final, não o começo da função.

- [ ] **Step 4: Aplicar o brilho de cada tela**

Em `main.cpp`, o brilho é aplicado em cada transição. `screen_brightness` já pula o fade quando o valor não muda, então chamar sempre é seguro.

Em `setup()`, substituir `watch->openBL();` por:

```c
    watch->openBL();
    screen_brightness(g_settings.brightnessClock, false);
```

Atenção: `settings_load` precisa vir **antes** dessa linha. Se `openBL()` estiver antes de `settings_load` no arquivo, mova a chamada de brilho para depois do `settings_load`.

Em `enterPanel()`, como primeira linha:

```c
    screen_brightness(g_settings.brightnessPanel, true);
```

Em `enterConfig()`, como primeira linha:

```c
    screen_brightness(g_settings.brightnessPanel, true);
```

E no `loop()`, no ramo que volta ao relógio (o `else` do bloco de toque), antes de `clock_view_draw`:

```c
            screen_brightness(g_settings.brightnessClock, true);
```

O ramo da alternância entre dispositivos (`previous == STATE_PAINEL`) **não** recebe chamada nenhuma: a tela já está no brilho do painel e continua nele. Acrescentar uma ali não quebraria nada, mas sugeriria uma transição que não existe.

- [ ] **Step 5: Verificar no aparelho**

```bash
cd twatch
pio run -t upload
```

Roteiro:

1. O relógio acende mais escuro que o painel; a transição é um fade, não um corte.
2. Com os dois Bitaxe ligados, abrir o painel **não** vibra.
3. Desligar um Bitaxe e abrir o painel: uma vibração curta.
4. Pelo portal, pôr brilho do relógio em 100 e conferir que a diferença some.

- [ ] **Step 6: Commit**

```bash
git add twatch/src/main.cpp
git commit -m "feat: add haptic alert and per-screen brightness"
```

---

### Task 4: Contador de passos no relógio

**Files:**
- Modify: `twatch/src/clock_view.h`
- Modify: `twatch/src/clock_view.cpp`
- Modify: `twatch/src/main.cpp`

**Interfaces:**
- Consumes: `watch->bma->enableFeature(BMA423_STEP_CNTR, true)`, `watch->bma->getCounter()`.
- Produces: campo `uint32_t steps` em `ClockViewModel`.

**Risco conhecido, tratado no Step 4:** o BMA423 é reconfigurado por `watch->begin()` a cada boot, e o deep sleep reexecuta o `setup()`. O contador do chip pode zerar a cada despertar. O Step 4 verifica isso no aparelho e só então decide se o acumulador em RTC memory é necessário.

- [ ] **Step 1: Acrescentar o campo ao modelo**

Em `twatch/src/clock_view.h`, no `struct ClockViewModel`, ao final:

```c
    uint32_t steps;
```

- [ ] **Step 2: Desenhar ao lado da bateria**

Os passos **não** podem dividir a linha da pilha, e **não** podem ficar abaixo dela na posição atual. As duas restrições, com os números:

- Mesma linha: com a pilha em `x=62`, a porcentagem é desenhada em `x + bodyW + 12 = 136` e `"100%"` chega a ~182. Sobra até `GRID_RIGHT` (224) uma faixa de 42 px; `"8432 passos"` mede ~97 px em `FONT_LABEL`. Não cabe.
- Abaixo dela: `CONFIG_STRIP_TOP_Y` é **192** e `clock_hit` devolve `HIT_CONFIG` para todo `y >= 192` — há uma divisória desenhada nessa altura e a engrenagem em y=214. Qualquer texto ali dentro fica **dentro do botão de config**: sobrepõe a engrenagem e, ao ser tocado, abre o portal. Entre a base da pilha (180) e a divisória (192) sobram 12 px, e a caixa de linha da `FONT_LABEL` ocupa 22.

E também **não** dá para subir a pilha e empilhar os dois acima da divisória. A tentativa foi feita e reprovada: `clock_view_draw` desenha a pilha **antes** de bifurcar entre hora válida e inválida (de propósito — o comentário no código diz que a carga é o que mais importa na tela de primeiro boot), e o caminho de hora inválida escreve `"para acertar a hora"` em y=140, caixa ~134..152. Com a pilha em 141..163 elas se cruzam, e como esses `drawString` usam `setTextColor(cor, THEME_BG)` o retângulo opaco de fundo **apaga** a parte de baixo da pilha. Entre 152 e a divisória (192) sobram 40 px, e duas caixas de linha precisam de 44: não cabe.

A solução é horizontal, não vertical: **os passos vão para a linha da data**, que está quase vazia, e a pilha **fica onde está**.

Em `twatch/src/clock_view.cpp`, no caminho de hora válida, trocar o desenho centralizado da data por data e passos nas duas bordas da grade:

```c
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
```

A conta: `"10/08"` à esquerda ocupa ~16..66; `"12345 passos"` à direita, ~112..224. Sobra folga mesmo com cinco dígitos.

Nada muda no caminho de hora inválida (ele não desenha passos: antes de a hora ser acertada aquela tela é instrução), nem abaixo de `CONFIG_STRIP_TOP_Y`, nem a posição da pilha. Cuidado só com o `setTextDatum`: a hora heroi é desenhada **antes** deste bloco e depende do `MC_DATUM` posto acima da bifurcação — leia a ordem real das chamadas antes de mexer.

- [ ] **Step 3: Ligar o contador e alimentar o modelo**

Em `twatch/src/main.cpp`, em `setup()`, depois de `watch->motor_begin();`:

```c
    // O contador de passos do BMA423 nao vem ligado; begin() so inicializa o
    // acelerometro. Idempotente a cada boot.
    watch->bma->enableFeature(BMA423_STEP_CNTR, true);
```

Em `clockModel()`, acrescentar:

```c
    vm.steps = watch->bma->getCounter();
```

- [ ] **Step 4: Verificar a persistência no aparelho**

```bash
cd twatch
pio run -t upload
```

**Este passo é humano.** Quem implementa não consegue andar com o relógio no pulso, e a decisão seguinte depende do resultado: se o contador do chip já persiste e o acumulador for acrescentado assim mesmo, os passos passam a ser contados em dobro a cada despertar (`g_stepsBase += getCounter()` ao dormir, e `getCounter()` continua com o valor antigo ao acordar). Grave o firmware, entregue os Steps 1-3 com status DONE_WITH_CONCERNS e **pare aqui** — não implemente o acumulador por precaução.

Roteiro (para o usuário):

1. Ande alguns passos com o relógio e confirme que o número sobe.
2. Deixe o relógio dormir (não toque nele até a tela apagar) e acorde-o com o toque.
3. **Confira se o contador manteve o valor.**

Se — e só se — o contador zerou, acrescente um acumulador em RTC memory. Em `main.cpp`, junto das outras variáveis `RTC_DATA_ATTR`:

```c
// O BMA423 e reconfigurado por watch->begin() a cada boot, e o deep sleep
// reexecuta o setup(). Sem acumular aqui, os passos zerariam a cada despertar.
RTC_DATA_ATTR static uint32_t g_stepsBase = 0;
```

E em `clockModel()`, trocar a linha dos passos por:

```c
    vm.steps = g_stepsBase + watch->bma->getCounter();
```

E em `goToSleep()`, antes de dormir:

```c
    g_stepsBase += watch->bma->getCounter();
```

Regrave e repita o roteiro para confirmar que o valor agora sobrevive ao ciclo de sono.

**Se você chegou a acrescentar essa variável**, incremente também `SNAPSHOT_MAGIC` em `src/snapshot_store.h`. Uma variável `RTC_DATA_ATTR` nova desloca o endereço das que já existem, e o cache dos Bitaxe vive nesse mesmo espaço: sem o incremento, um despertar logo após a regravação poderia ler lixo no offset antigo e aceitá-lo como snapshot válido. O magic existe exatamente para invalidar o cache quando o formato muda — e o endereço faz parte do formato.

- [ ] **Step 5: Commit**

```bash
git add twatch/src/clock_view.h twatch/src/clock_view.cpp twatch/src/main.cpp
git commit -m "feat: show step counter on clock face"
```

---

### Task 5: Vigia periódica

**Files:**
- Modify: `twatch/src/main.cpp`

**Interfaces:**
- Consumes: `watchdog_should_run`, `watchdog_should_alert`, `pollDevices` (da Task 3), `esp_sleep_enable_timer_wakeup`, `esp_sleep_get_wakeup_cause`.
- Produces: `static void runWatchdogCycle(void)`.

- [ ] **Step 1: Armar o temporizador ao dormir**

Em `twatch/src/main.cpp`, dentro de `goToSleep()`, junto das outras fontes de despertar (depois das duas linhas de `esp_sleep_enable_ext0_wakeup` e `esp_sleep_enable_ext1_wakeup`):

```c
    // Terceira fonte, independente das duas acima: o timer nao disputa modo
    // com ext0/ext1, entao convive com o toque e com o botao PEK sem custo.
    if (watchdog_should_run(g_settings.watchIntervalMin, batteryNow(), g_settings.batteryMinPct)) {
        esp_sleep_enable_timer_wakeup((uint64_t) g_settings.watchIntervalMin * 60ULL * 1000000ULL);
    }
```

- [ ] **Step 2: Escrever o ciclo silencioso**

A busca em si é a `pollDevices()` que a Task 3 extraiu — **não** reescreva o laço aqui. A vigia só acrescenta o que é dela: abrir e fechar o rádio, e vibrar diferente.

**Antes de usar `alertBuzz(2)`, corrija o intervalo entre pulsos.** O `alertBuzz` da Task 3 espera 150 ms entre um `shake()` e o seguinte, mas `Motor::onec()` liga o motor por **200 ms** por padrão (`TTGO_TWatch_Library/src/drive/tft/bl.h:106`) e re-arma o `Ticker` a cada chamada. Com 150 ms, o segundo pulso chega antes de o primeiro terminar: o motor nunca desliga no meio e o usuário sente **uma** vibração longa, não duas — exatamente a distinção que esta task existe para criar. Em `alertBuzz`, troque o `delay(150)` por:

```c
            // Maior que os 200 ms que Motor::onec() mantem o motor ligado, senao
            // o pulso seguinte re-arma o Ticker antes de o anterior terminar e as
            // duas vibracoes viram uma so, longa.
            delay(300);
```

Acrescentar acima de `setup()`:

```c
// Ciclo de vigia: acontece com a tela APAGADA. Consulta os aparelhos, atualiza
// o cache e vibra se algo caiu. O usuario percebe pela vibracao, ou pelos dados
// ja frescos quando abrir o painel depois.
static void runWatchdogCycle()
{
    if (!net_connect(g_settings.wifiSsid, g_settings.wifiPass, g_settings.wifiTimeoutMs)) {
        // Sem rede nao ha o que afirmar sobre os mineradores: vibrar aqui
        // seria alarme falso toda vez que o WiFi oscilasse.
        net_disconnect();
        return;
    }

    WatchdogInputs health = {};
    pollDevices(health);
    net_disconnect();

    if (watchdog_should_alert(health)) {
        // Duas vibracoes: distingue do alerta de uma so que o painel da ao
        // abrir, e e perceptivel com o aparelho no pulso e a tela apagada.
        alertBuzz(2);
    }
}
```

Efeito colateral desejado de reusar `pollDevices`: a vigia também marca `g_justFetched`. Como `enterPanel()` zera esse array no início de toda sessão, isso não faz o painel mentir sobre idade — mas quer dizer que abrir o painel logo após um ciclo de vigia já encontra o cache quente.

- [ ] **Step 3: Desviar o boot quando o despertar for do timer**

Em `setup()`, **antes** de `watch->openBL()` e de qualquer desenho, e depois de `settings_load` e da inicialização de energia/motor:

```c
    // Despertar por timer nao acende a tela: e a vigia rodando em segundo
    // plano. goToSleep() nao retorna.
    if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER) {
        runWatchdogCycle();
        goToSleep();
    }
```

A ordem correta dentro de `setup()` passa a ser: `Serial.begin` → `watch->begin()` → `settings_load` → `adc1Enable` → `enableIRQ`/`clearIRQ` → `motor_begin` → `enableFeature(BMA423_STEP_CNTR)` → **ramo do timer** → `openBL` → `screen_brightness` → `screen_begin` → logs de PSRAM → estado inicial e `clock_view_draw`.

- [ ] **Step 4: Verificar no aparelho**

```bash
cd twatch
pio run -t upload
pio device monitor
```

Roteiro:

1. Pelo portal, ponha a vigia em **5** minutos e salve.
2. Deixe o relógio dormir. Com os dois Bitaxe ligados, ele deve acordar em ~5 min, ficar com a **tela apagada** e não vibrar. O log serial mostra o boot.
3. Desligue um Bitaxe e espere o próximo ciclo: **duas vibrações**, tela apagada.
4. Acorde pelo toque e abra o painel: os dados já estão frescos, sem espera de rede visível.
5. Volte a vigia para **0** pelo portal e confirme que o relógio deixa de acordar sozinho.

- [ ] **Step 5: Commit**

```bash
git add twatch/src/main.cpp
git commit -m "feat: add periodic background watchdog with haptic alert"
```

---

### Task 6: Documentação

**Files:**
- Modify: `twatch/README.md`

- [ ] **Step 1: Documentar os recursos novos**

Acrescentar ao `twatch/README.md`, depois da seção "Dois Bitaxe":

```markdown
## Recursos do relogio

**Vibracao.** O motor so dispara quando ha problema: uma vibracao ao abrir o
painel se algum Bitaxe nao respondeu ou parou de minerar, duas quando a vigia
periodica detecta isso com a tela apagada. Silencio significa que esta tudo bem.

**Brilho por tela.** O relogio acende mais escuro que o painel, com fade na
transicao. Os dois valores sao configuraveis no portal, de 10 a 100 por cento.

**Passos.** O contador do BMA423 aparece ao lado da bateria. Nao tem nada a ver
com mineracao — e o sensor que o relogio ja tem e que estava ocioso.

**Vigia periodica.** Desligada por padrao. Ligada, o relogio acorda sozinho no
intervalo configurado (5 a 240 min), consulta os dois Bitaxe **sem acender a
tela** e vibra duas vezes se algum caiu. E o unico recurso que gasta bateria sem
o usuario pedir: cada ciclo mantem o radio ligado por volta de 5 a 8 segundos,
e a vigia se suspende sozinha quando a carga cai abaixo do minimo configurado.

### Por que nao ha "levantar o pulso para acender"

O BMA423 interrompe no GPIO39 ativo em nivel **alto**. O ESP32 classico tem
`ext0` (um pino so) e `ext1` (um modo unico para toda a mascara: `ALL_LOW` ou
`ANY_HIGH`). O `ext0` ja e o toque, ativo em nivel baixo, e o `ext1` e o botao
PEK em `ALL_LOW`. Um terceiro pino ativo em alto nao cabe sem sacrificar uma
das duas fontes atuais — e nenhuma das duas vale o troco.

O despertar por timer da vigia nao sofre desse problema: e uma fonte
independente, que nao disputa modo com `ext0` nem com `ext1`.
```

Atualizar também a seção "Estado atual" com a contagem final: **nove suítes** — as seis originais mais `test_battery`, `test_cycle` e `test_watchdog`.

- [ ] **Step 2: Rodar a bateria inteira**

```bash
cd twatch
pio test --upload-port COM5
```

Esperado: nove suítes, todas PASS.

- [ ] **Step 3: Commit**

```bash
git add twatch/README.md
git commit -m "docs: document watch features and the wake-source tradeoff"
```

---

## Verificação final da fase

1. Painel com tudo saudável não vibra; com um Bitaxe caído, vibra uma vez.
2. Relógio visivelmente mais escuro que o painel, com fade entre eles.
3. Contador de passos sobe e **sobrevive** a um ciclo de deep sleep.
4. Vigia em 5 min acorda o relógio sem acender a tela e vibra duas vezes quando um Bitaxe cai.
5. Vigia em 0 impede qualquer despertar espontâneo.
6. Portal recusa brilho abaixo de 10 e vigia entre 1 e 4 minutos.
7. `pio test --upload-port COM5` fecha em nove suítes, todas verdes.
