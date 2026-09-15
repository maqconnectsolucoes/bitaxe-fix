# Painel Bitaxe no T-Watch — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Transformar o projeto `twatch/` num relógio de duas telas — mostrador de hora e painel Bitaxe — que dorme entre consultas e pinta dados do cache instantaneamente.

**Architecture:** Máquina de estados com quatro estados (DEEP_SLEEP, RELOGIO, PAINEL) onde o WiFi só existe no painel. A lógica pura (parse de JSON, cálculo de J/TH, idade do cache, transições de estado) vive em `lib/bitaxe_core/` e roda em testes nativos no PC; o código que toca hardware fica em `src/`. O último estado conhecido é guardado em RTC slow memory para atravessar o deep sleep.

**Tech Stack:** PlatformIO, Arduino core esp32 2.0.17 (`espressif32@6.9.0`), TTGO_TWatch_Library, ArduinoJson 7, Unity para testes nativos.

## Global Constraints

- Alvo: TTGO T-Watch 2020 V1, ESP32-D0WDQ6-V3, PMU **AXP202** (não AXP2101), tela 240x240
- Plataforma travada em `espressif32@6.9.0` (entrega Arduino core 2.0.17)
- Biblioteca: `https://github.com/Xinyuan-LilyGO/TTGO_TWatch_Library.git` (sem hífen entre TTGO e TWatch)
- `partitions.csv` reproduz o layout do aparelho e **não deve ser alterado** — é o que mantém o backup restaurável
- Toda requisição à API do Bitaxe **deve** enviar o header `Origin`; sem ele a resposta é `401`
- Campos booleanos da API trafegam como `0`/`1` numérico, não booleano JSON
- Structs gravadas em `RTC_DATA_ATTR` devem ser POD: **sem `String`, sem ponteiros, sem `std::` com heap**
- Limiares fixados na spec: J/TH oculto quando `hashRate_10m < 1.0` Gh/s; painel recusa WiFi com bateria `< 15%`
- Timeouts: RELOGIO dorme após 5 s sem toque; PAINEL após 15 s; conexão WiFi aborta em 8 s
- Nada de alerta de overheat, vibração ou `motor_begin()` — descartados por decisão de design
- **Testes rodam no aparelho** (`pio test -e t-watch-2020-v1`), não em `platform = native`: esta máquina não tem compilador de host. O relógio precisa estar no COM5.
- **Sempre passe `--upload-port COM5`** nos comandos `pio test` e `pio run -t upload`: há outro dispositivo USB serial nesta máquina e o PlatformIO autodetecta COM3, ignorando o `test_port` do `platformio.ini`.
- **Instalação de pacotes grandes:** o downloader do PlatformIO reinicia do zero a cada queda de conexão e não converge aqui. Para pacotes grandes, baixe com `curl.exe -L -C - --retry 10 --retry-all-errors`, extraia à mão em `~/.platformio/packages/<nome>` e escreva o `.piopm` com a ferramenta Write (nunca `Out-File`, que grava BOM e quebra o parser JSON). Verificado nesta sessão para o toolchain e o framework.

---

### Task 1: Desbloquear o toolchain e provar que o projeto compila

O GCC 8.4.0 já está extraído em `~/.platformio/packages/toolchain-xtensa-esp32`, mas o PlatformIO Core 6 rastreia pacotes instalados por um arquivo `.piopm`, não pelo `package.json`. Sem ele o pacote é invisível e o build tenta rebaixar 120 MB de um mirror instável que vem quebrando no meio.

**Files:**
- Modify: `twatch/platformio.ini` (nenhuma mudança se o registro funcionar; ver Step 4)

**Interfaces:**
- Consumes: nada
- Produces: um build funcional — todas as tarefas seguintes dependem disto

- [ ] **Step 1: Registrar o toolchain local pela via oficial**

O jeito documentado de instalar um pacote local é `file://`, e ele faz o PlatformIO gerar o `.piopm` correto sozinho — melhor do que escrever o arquivo à mão e torcer pelo schema.

```powershell
$pio = "$env:USERPROFILE\AppData\Local\Programs\Python\Python314\Scripts\pio.exe"
$tc  = "$env:USERPROFILE\.platformio\packages\toolchain-xtensa-esp32"
& $pio pkg install --global --tool "file://$tc"
```

- [ ] **Step 2: Confirmar que o PlatformIO enxerga o pacote**

```powershell
& $pio pkg list --global --only-tools
```

Esperado: uma linha citando `toolchain-xtensa-esp32 @ 8.4.0+2021r2-patch5`. Se não aparecer, siga para o Step 3; se aparecer, pule para o Step 4.

- [ ] **Step 3 (fallback): Escrever o `.piopm` à mão**

Só execute se o Step 2 falhou. Crie `~/.platformio/packages/toolchain-xtensa-esp32/.piopm` **sem BOM** (o `Out-File -Encoding utf8` do PowerShell 5.1 grava BOM e o PlatformIO rejeita o JSON):

```json
{
  "type": "tool",
  "name": "toolchain-xtensa-esp32",
  "version": "8.4.0+2021r2-patch5",
  "spec": {
    "owner": "espressif",
    "id": null,
    "name": "toolchain-xtensa-esp32",
    "requirements": null,
    "uri": null
  }
}
```

Verifique a ausência de BOM: o primeiro byte deve ser `123` (`{`).

```powershell
$b = [System.IO.File]::ReadAllBytes("$tc\.piopm"); $b[0]
```

- [ ] **Step 4: Rodar o build**

```powershell
cd C:\bitaxe\ESP-Miner-master\twatch
& $pio --no-ansi run
```

Esperado: o toolchain **não** é baixado de novo. O build ainda precisa buscar `framework-arduinoespressif32` e clonar a TTGO_TWatch_Library — isso pode demorar e, se a rede quebrar, repita o comando (o PlatformIO retoma o que já baixou).

- [ ] **Step 5: Corrigir os erros de compilação que aparecerem**

O `main.cpp` atual nunca passou pelo compilador. Erros esperados e como tratá-los:

- Símbolos da TTGO ausentes → confira o nome exato no header da biblioteca já baixada em `.pio/libdeps/t-watch-2020-v1/`
- `AXP202_BATT_VOL_ADC1` não declarado → o header do AXP está em `src/drive/axp/axp20x.h` da biblioteca; confirme que `LilyGoWatch.h` o inclui

Não "conserte" removendo funcionalidade: o objetivo deste passo é só provar o toolchain.

- [ ] **Step 6: Commit**

```bash
git add twatch/platformio.ini
git commit -m "build: get T-Watch project compiling with local GCC 8.4.0 toolchain"
```

---

### Task 2: Ambiente de teste nativo e `BitaxeStatus` como POD

**Files:**
- Create: `twatch/lib/bitaxe_core/bitaxe_status.h`
- Modify: `twatch/platformio.ini` (habilitar Unity no env do alvo)
- Test: `twatch/test/test_status/test_status.cpp`

**Interfaces:**
- Produces: `struct BitaxeStatus` (POD) e `BITAXE_STR_LEN`, consumidos por todas as tarefas seguintes

- [ ] **Step 1: Escrever o teste que falha**

`twatch/test/test_status/test_status.cpp`:

```cpp
#include <Arduino.h>
#include <unity.h>
#include <type_traits>
#include "bitaxe_status.h"

void test_status_is_pod_so_it_survives_deep_sleep(void)
{
    // RTC_DATA_ATTR exige POD: String ou ponteiro aqui viraria lixo no despertar.
    TEST_ASSERT_TRUE(std::is_trivially_copyable<BitaxeStatus>::value);
    TEST_ASSERT_TRUE(std::is_standard_layout<BitaxeStatus>::value);
}

void test_status_fits_in_rtc_slow_memory(void)
{
    // A RTC slow memory tem 8 KB no total e é compartilhada.
    TEST_ASSERT_LESS_THAN(512, (int) sizeof(BitaxeStatus));
}

void setUp(void) {}
void tearDown(void) {}

// Testes no alvo usam setup()/loop(), não main(). O delay inicial dá tempo de
// a serial subir antes de os resultados saírem.
void setup()
{
    delay(2000);
    UNITY_BEGIN();
    RUN_TEST(test_status_is_pod_so_it_survives_deep_sleep);
    RUN_TEST(test_status_fits_in_rtc_slow_memory);
    UNITY_END();
}

void loop() {}
```

- [ ] **Step 2: Habilitar testes no alvo**

Esta máquina **não tem compilador de host** — nem MinGW, nem MSVC, nem clang. Por isso os testes rodam no próprio relógio, e não em `platform = native`. O toolchain do ESP32 já funciona, então isso não exige nenhum download novo.

Acrescente ao bloco `[env:t-watch-2020-v1]` já existente em `twatch/platformio.ini`:

```ini
test_framework = unity
test_port = COM5
test_speed = 115200
```

O código puro vive em `lib/bitaxe_core/`, que o PlatformIO compila e linka automaticamente tanto no firmware quanto nos binários de teste.

**O relógio precisa estar ligado no COM5** para rodar os testes: cada suíte é compilada, gravada no aparelho e executada lá, com os resultados voltando pela serial. Um ciclo leva cerca de um minuto, não segundos.

- [ ] **Step 3: Rodar o teste para vê-lo falhar**

```powershell
& $pio test -e t-watch-2020-v1
```

Esperado: FALHA na compilação, `bitaxe_status.h: No such file or directory`.

- [ ] **Step 4: Criar a struct**

`twatch/lib/bitaxe_core/bitaxe_status.h`:

```cpp
#pragma once

#include <stdint.h>

// Comprimento dos campos de texto. bestDiff chega como "1.2T"/"415M";
// hostname é o nome do aparelho na rede.
#define BITAXE_STR_LEN 24

// POD por exigência: esta struct é gravada em RTC_DATA_ATTR e precisa
// atravessar o deep sleep, quando o heap deixa de existir.
struct BitaxeStatus
{
    float hashRate10m;        // Gh/s, média de 10 min
    float power;              // W
    float frequency;          // MHz
    float coreVoltageActual;  // mV
    uint32_t sharesAccepted;
    char bestDiff[BITAXE_STR_LEN];
    char hostname[BITAXE_STR_LEN];
};
```

- [ ] **Step 5: Rodar os testes e vê-los passar**

```powershell
& $pio test -e t-watch-2020-v1
```

Esperado: 2 testes, ambos PASS.

- [ ] **Step 6: Commit**

```bash
git add twatch/lib/bitaxe_core/bitaxe_status.h twatch/test/test_status/test_status.cpp twatch/platformio.ini
git commit -m "test: enable on-target Unity tests and add POD BitaxeStatus"
```

---

### Task 3: `bitaxe_parse` puro

**Files:**
- Create: `twatch/lib/bitaxe_core/bitaxe_parse.h`, `twatch/lib/bitaxe_core/bitaxe_parse.cpp`
- Test: `twatch/test/test_parse/test_parse.cpp`

**Interfaces:**
- Consumes: `BitaxeStatus` (Task 2)
- Produces: `bool bitaxe_parse(const char *json, BitaxeStatus &out)` — usada pela Task 7

- [ ] **Step 1: Escrever os testes que falham**

`twatch/test/test_parse/test_parse.cpp`:

```cpp
#include <Arduino.h>
#include <unity.h>
#include <string.h>
#include "bitaxe_parse.h"

// Recorte de uma resposta real de /api/system/info.
static const char *VALID_JSON =
    "{\"hashRate\":1180.5,\"hashRate_10m\":1204.8,\"power\":17.2,"
    "\"frequency\":490,\"coreVoltageActual\":1150,\"sharesAccepted\":8421,"
    "\"bestDiff\":\"1.2T\",\"hostname\":\"bitaxe\",\"overheat_mode\":0}";

void test_parse_reads_expected_fields(void)
{
    BitaxeStatus s;
    TEST_ASSERT_TRUE(bitaxe_parse(VALID_JSON, s));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1204.8f, s.hashRate10m);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 17.2f, s.power);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 490.0f, s.frequency);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1150.0f, s.coreVoltageActual);
    TEST_ASSERT_EQUAL_UINT32(8421, s.sharesAccepted);
    TEST_ASSERT_EQUAL_STRING("1.2T", s.bestDiff);
    TEST_ASSERT_EQUAL_STRING("bitaxe", s.hostname);
}

void test_parse_rejects_truncated_json(void)
{
    BitaxeStatus s;
    TEST_ASSERT_FALSE(bitaxe_parse("{\"hashRate_10m\":120", s));
}

void test_parse_rejects_empty_input(void)
{
    BitaxeStatus s;
    TEST_ASSERT_FALSE(bitaxe_parse("", s));
}

void test_optional_fields_absent_still_parses(void)
{
    BitaxeStatus s;
    TEST_ASSERT_TRUE(bitaxe_parse("{\"hashRate_10m\":900.0,\"power\":15.5}", s));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 900.0f, s.hashRate10m);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 15.5f, s.power);
    TEST_ASSERT_EQUAL_UINT32(0, s.sharesAccepted);
    TEST_ASSERT_EQUAL_STRING("", s.bestDiff);
}

void test_missing_essential_field_is_rejected(void)
{
    BitaxeStatus s;
    TEST_ASSERT_FALSE(bitaxe_parse("{\"power\":17.2}", s));          // sem hashRate_10m
    TEST_ASSERT_FALSE(bitaxe_parse("{\"hashRate_10m\":900.0}", s));  // sem power
    TEST_ASSERT_FALSE(bitaxe_parse("{}", s));
}

void test_rejection_leaves_caller_struct_untouched(void)
{
    // O chamador guarda um cache válido; uma resposta ruim não pode corrompê-lo.
    BitaxeStatus s = {};
    s.hashRate10m = 1204.8f;
    s.power = 17.2f;
    strncpy(s.bestDiff, "1.2T", BITAXE_STR_LEN - 1);

    TEST_ASSERT_FALSE(bitaxe_parse("{\"power\":17.2}", s));
    TEST_ASSERT_FALSE(bitaxe_parse("nao e json", s));

    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1204.8f, s.hashRate10m);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 17.2f, s.power);
    TEST_ASSERT_EQUAL_STRING("1.2T", s.bestDiff);
}

void test_long_strings_are_truncated_not_overflowed(void)
{
    BitaxeStatus s;
    const char *json =
        "{\"hostname\":\"um-nome-de-host-absurdamente-longo-que-nao-cabe\"}";
    TEST_ASSERT_TRUE(bitaxe_parse(json, s));
    TEST_ASSERT_EQUAL_INT(BITAXE_STR_LEN - 1, (int) strlen(s.hostname));
}

void setUp(void) {}
void tearDown(void) {}

// Testes no alvo usam setup()/loop(), não main().
void setup()
{
    delay(2000);
    UNITY_BEGIN();
    RUN_TEST(test_parse_reads_expected_fields);
    RUN_TEST(test_parse_rejects_truncated_json);
    RUN_TEST(test_parse_rejects_empty_input);
    RUN_TEST(test_optional_fields_absent_still_parses);
    RUN_TEST(test_missing_essential_field_is_rejected);
    RUN_TEST(test_rejection_leaves_caller_struct_untouched);
    RUN_TEST(test_long_strings_are_truncated_not_overflowed);
    UNITY_END();
}

void loop() {}
```

- [ ] **Step 2: Rodar para ver falhar**

```powershell
& $pio test -e t-watch-2020-v1 -f test_parse
```

Esperado: FALHA de compilação, `bitaxe_parse.h: No such file or directory`.

- [ ] **Step 3: Implementar**

`twatch/lib/bitaxe_core/bitaxe_parse.h`:

```cpp
#pragma once

#include "bitaxe_status.h"

// Devolve false e deixa `out` intocada se o JSON for inválido OU se faltar um
// campo essencial (`hashRate_10m`, `power`). O chamador guarda um cache válido
// que não pode ser corrompido: meio dado é pior que dado velho.
// Campos opcionais ausentes viram zero ou string vazia, nunca lixo.
bool bitaxe_parse(const char *json, BitaxeStatus &out);
```

`twatch/lib/bitaxe_core/bitaxe_parse.cpp`:

```cpp
#include "bitaxe_parse.h"

#include <ArduinoJson.h>
#include <string.h>

static void copyField(char *dest, size_t cap, const char *src)
{
    if (!src) {
        dest[0] = '\0';
        return;
    }
    strncpy(dest, src, cap - 1);
    dest[cap - 1] = '\0';
}

bool bitaxe_parse(const char *json, BitaxeStatus &out)
{
    if (!json || json[0] == '\0') {
        return false;
    }

    JsonDocument doc;
    if (deserializeJson(doc, json)) {
        return false;  // preserva o cache do chamador
    }

    // A spec manda descartar resposta incompleta: sobrescrever o cache com
    // zeros faria o painel mostrar um minerador parado que não está parado.
    if (!doc["hashRate_10m"].is<float>() || !doc["power"].is<float>()) {
        return false;
    }

    BitaxeStatus s = {};
    s.hashRate10m = doc["hashRate_10m"] | 0.0f;
    s.power = doc["power"] | 0.0f;
    s.frequency = doc["frequency"] | 0.0f;
    s.coreVoltageActual = doc["coreVoltageActual"] | 0.0f;
    s.sharesAccepted = doc["sharesAccepted"] | 0u;
    copyField(s.bestDiff, BITAXE_STR_LEN, doc["bestDiff"] | (const char *) nullptr);
    copyField(s.hostname, BITAXE_STR_LEN, doc["hostname"] | (const char *) nullptr);

    out = s;
    return true;
}
```

- [ ] **Step 4: Rodar e ver passar**

```powershell
& $pio test -e t-watch-2020-v1 -f test_parse
```

Esperado: 5 testes PASS.

- [ ] **Step 5: Commit**

```bash
git add twatch/lib/bitaxe_core/bitaxe_parse.h twatch/lib/bitaxe_core/bitaxe_parse.cpp twatch/test/test_parse/
git commit -m "feat: add pure bitaxe_parse with native tests"
```

---

### Task 4: Métricas — J/TH, idade e formatação

**Files:**
- Create: `twatch/lib/bitaxe_core/metrics.h`, `twatch/lib/bitaxe_core/metrics.cpp`
- Test: `twatch/test/test_metrics/test_metrics.cpp`

**Interfaces:**
- Consumes: `BitaxeStatus` (Task 2)
- Produces: `bool metrics_efficiency(const BitaxeStatus &, float &)`, `enum Freshness`, `Freshness metrics_freshness(uint32_t)`, `void metrics_format_hashrate(float, char *, size_t)` — usados pela Task 9

- [ ] **Step 1: Escrever os testes que falham**

`twatch/test/test_metrics/test_metrics.cpp`:

```cpp
#include <Arduino.h>
#include <unity.h>
#include <string.h>
#include "metrics.h"

void test_efficiency_is_watts_per_terahash(void)
{
    BitaxeStatus s = {};
    s.power = 17.2f;
    s.hashRate10m = 1204.8f;  // Gh/s = 1.2048 Th/s
    float jth = 0.0f;
    TEST_ASSERT_TRUE(metrics_efficiency(s, jth));
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 14.28f, jth);
}

void test_efficiency_hidden_when_hashrate_below_threshold(void)
{
    BitaxeStatus s = {};
    s.power = 17.2f;
    s.hashRate10m = 0.0f;  // minerador parado: divisão por zero
    float jth = 0.0f;
    TEST_ASSERT_FALSE(metrics_efficiency(s, jth));
}

void test_efficiency_hidden_just_below_one_ghs(void)
{
    BitaxeStatus s = {};
    s.power = 5.0f;
    s.hashRate10m = 0.99f;
    float jth = 0.0f;
    TEST_ASSERT_FALSE(metrics_efficiency(s, jth));
}

void test_freshness_thresholds(void)
{
    TEST_ASSERT_EQUAL(FRESHNESS_NOW, metrics_freshness(0));
    TEST_ASSERT_EQUAL(FRESHNESS_NOW, metrics_freshness(59));
    TEST_ASSERT_EQUAL(FRESHNESS_AGING, metrics_freshness(60));
    TEST_ASSERT_EQUAL(FRESHNESS_AGING, metrics_freshness(599));
    TEST_ASSERT_EQUAL(FRESHNESS_STALE, metrics_freshness(600));
}

void test_hashrate_switches_to_terahash_at_1000(void)
{
    char buf[24];
    metrics_format_hashrate(999.0f, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("999.0 Gh/s", buf);
    metrics_format_hashrate(1000.0f, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("1.00 Th/s", buf);
}

void setUp(void) {}
void tearDown(void) {}

// Testes no alvo usam setup()/loop(), não main().
void setup()
{
    delay(2000);
    UNITY_BEGIN();
    RUN_TEST(test_efficiency_is_watts_per_terahash);
    RUN_TEST(test_efficiency_hidden_when_hashrate_below_threshold);
    RUN_TEST(test_efficiency_hidden_just_below_one_ghs);
    RUN_TEST(test_freshness_thresholds);
    RUN_TEST(test_hashrate_switches_to_terahash_at_1000);
    UNITY_END();
}

void loop() {}
```

- [ ] **Step 2: Rodar para ver falhar**

```powershell
& $pio test -e t-watch-2020-v1 -f test_metrics
```

Esperado: FALHA, `metrics.h: No such file or directory`.

- [ ] **Step 3: Implementar**

`twatch/lib/bitaxe_core/metrics.h`:

```cpp
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "bitaxe_status.h"

// Abaixo disto o J/TH não é exibido: a divisão estouraria e, com o minerador
// parado, o número não significaria nada.
#define METRICS_MIN_HASHRATE_GHS 1.0f

enum Freshness
{
    FRESHNESS_NOW,    // < 60 s: idade oculta
    FRESHNESS_AGING,  // 60 s a 10 min: idade visível
    FRESHNESS_STALE,  // >= 10 min: números acinzentados
};

bool metrics_efficiency(const BitaxeStatus &s, float &joulesPerTerahash);
Freshness metrics_freshness(uint32_t ageSeconds);
void metrics_format_hashrate(float ghs, char *buf, size_t cap);
```

`twatch/lib/bitaxe_core/metrics.cpp`:

```cpp
#include "metrics.h"

#include <stdio.h>

bool metrics_efficiency(const BitaxeStatus &s, float &joulesPerTerahash)
{
    if (s.hashRate10m < METRICS_MIN_HASHRATE_GHS) {
        return false;
    }
    // A API entrega Gh/s e watts; J/TH = W / (Gh/s / 1000).
    joulesPerTerahash = s.power / (s.hashRate10m / 1000.0f);
    return true;
}

Freshness metrics_freshness(uint32_t ageSeconds)
{
    if (ageSeconds < 60) {
        return FRESHNESS_NOW;
    }
    if (ageSeconds < 600) {
        return FRESHNESS_AGING;
    }
    return FRESHNESS_STALE;
}

void metrics_format_hashrate(float ghs, char *buf, size_t cap)
{
    if (ghs >= 1000.0f) {
        snprintf(buf, cap, "%.2f Th/s", ghs / 1000.0f);
    } else {
        snprintf(buf, cap, "%.1f Gh/s", ghs);
    }
}
```

- [ ] **Step 4: Rodar e ver passar**

```powershell
& $pio test -e t-watch-2020-v1 -f test_metrics
```

Esperado: 5 testes PASS.

- [ ] **Step 5: Commit**

```bash
git add twatch/lib/bitaxe_core/metrics.h twatch/lib/bitaxe_core/metrics.cpp twatch/test/test_metrics/
git commit -m "feat: add efficiency, freshness and hashrate formatting with tests"
```

---

### Task 5: Máquina de estados

**Files:**
- Create: `twatch/lib/bitaxe_core/state_machine.h`, `twatch/lib/bitaxe_core/state_machine.cpp`
- Test: `twatch/test/test_state/test_state.cpp`

**Interfaces:**
- Produces: `enum AppState`, `enum AppEvent`, `AppState state_next(AppState, AppEvent, uint32_t idleMs)` — usada pela Task 10

- [ ] **Step 1: Escrever os testes que falham**

`twatch/test/test_state/test_state.cpp`:

```cpp
#include <Arduino.h>
#include <unity.h>
#include "state_machine.h"

void test_wake_from_sleep_goes_to_clock(void)
{
    TEST_ASSERT_EQUAL(STATE_RELOGIO, state_next(STATE_DEEP_SLEEP, EVENT_WAKE, 0));
}

void test_touch_on_clock_opens_panel(void)
{
    TEST_ASSERT_EQUAL(STATE_PAINEL, state_next(STATE_RELOGIO, EVENT_TOUCH, 0));
}

void test_touch_on_panel_returns_to_clock(void)
{
    TEST_ASSERT_EQUAL(STATE_RELOGIO, state_next(STATE_PAINEL, EVENT_TOUCH, 0));
}

void test_clock_sleeps_after_five_seconds_idle(void)
{
    TEST_ASSERT_EQUAL(STATE_RELOGIO, state_next(STATE_RELOGIO, EVENT_TICK, 4999));
    TEST_ASSERT_EQUAL(STATE_DEEP_SLEEP, state_next(STATE_RELOGIO, EVENT_TICK, 5000));
}

void test_panel_sleeps_after_fifteen_seconds_idle(void)
{
    TEST_ASSERT_EQUAL(STATE_PAINEL, state_next(STATE_PAINEL, EVENT_TICK, 14999));
    TEST_ASSERT_EQUAL(STATE_DEEP_SLEEP, state_next(STATE_PAINEL, EVENT_TICK, 15000));
}

void test_tick_never_wakes_from_sleep(void)
{
    // Só um evento externo acorda; o tempo passando, não.
    TEST_ASSERT_EQUAL(STATE_DEEP_SLEEP, state_next(STATE_DEEP_SLEEP, EVENT_TICK, 999999));
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
    UNITY_END();
}

void loop() {}
```

- [ ] **Step 2: Rodar para ver falhar**

```powershell
& $pio test -e t-watch-2020-v1 -f test_state
```

Esperado: FALHA, `state_machine.h: No such file or directory`.

- [ ] **Step 3: Implementar**

`twatch/lib/bitaxe_core/state_machine.h`:

```cpp
#pragma once

#include <stdint.h>

#define IDLE_TIMEOUT_RELOGIO_MS 5000
#define IDLE_TIMEOUT_PAINEL_MS 15000

enum AppState
{
    STATE_DEEP_SLEEP,
    STATE_RELOGIO,
    STATE_PAINEL,
};

enum AppEvent
{
    EVENT_WAKE,   // despertou do deep sleep (toque ou botão PEK)
    EVENT_TOUCH,  // toque com a tela já acesa
    EVENT_TICK,   // passagem de tempo
};

// Pura: recebe estado, evento e o tempo ocioso, devolve o próximo estado.
AppState state_next(AppState current, AppEvent event, uint32_t idleMs);
```

`twatch/lib/bitaxe_core/state_machine.cpp`:

```cpp
#include "state_machine.h"

AppState state_next(AppState current, AppEvent event, uint32_t idleMs)
{
    switch (current) {
    case STATE_DEEP_SLEEP:
        return event == EVENT_WAKE ? STATE_RELOGIO : STATE_DEEP_SLEEP;

    case STATE_RELOGIO:
        if (event == EVENT_TOUCH) {
            return STATE_PAINEL;
        }
        return idleMs >= IDLE_TIMEOUT_RELOGIO_MS ? STATE_DEEP_SLEEP : STATE_RELOGIO;

    case STATE_PAINEL:
        if (event == EVENT_TOUCH) {
            return STATE_RELOGIO;
        }
        return idleMs >= IDLE_TIMEOUT_PAINEL_MS ? STATE_DEEP_SLEEP : STATE_PAINEL;
    }
    return STATE_DEEP_SLEEP;
}
```

- [ ] **Step 4: Rodar e ver passar**

```powershell
& $pio test -e t-watch-2020-v1 -f test_state
```

Esperado: 6 testes PASS.

- [ ] **Step 5: Commit**

```bash
git add twatch/lib/bitaxe_core/state_machine.h twatch/lib/bitaxe_core/state_machine.cpp twatch/test/test_state/
git commit -m "feat: add app state machine with tests"
```

---

### Task 6: `snapshot_store` — cache na RTC slow memory

**Files:**
- Create: `twatch/src/snapshot_store.h`, `twatch/src/snapshot_store.cpp`

**Interfaces:**
- Consumes: `BitaxeStatus` (Task 2)
- Produces: `bool snapshot_load(BitaxeStatus &)`, `void snapshot_save(const BitaxeStatus &, uint32_t epoch)`, `uint32_t snapshot_age(uint32_t nowEpoch)` — usados pelas Tasks 9 e 10

Este módulo depende de `RTC_DATA_ATTR` (só existe no ESP32) e por isso vive em `src/`, fora do build nativo. A aritmética de idade que ele expõe é trivial; o comportamento interessante já foi testado em `metrics_freshness` (Task 4).

- [ ] **Step 1: Escrever o header**

`twatch/src/snapshot_store.h`:

```cpp
#pragma once

#include <stdint.h>

#include "bitaxe_status.h"

// Devolve false se nunca houve gravação (primeiro uso ou bateria zerada).
bool snapshot_load(BitaxeStatus &out);

// `epoch` é o horário Unix da resposta, lido do RTC.
void snapshot_save(const BitaxeStatus &s, uint32_t epoch);

// Segundos desde a gravação. Zero se não houver cache.
uint32_t snapshot_age(uint32_t nowEpoch);
```

- [ ] **Step 2: Implementar**

`twatch/src/snapshot_store.cpp`:

```cpp
#include "snapshot_store.h"

#include <esp_attr.h>  // RTC_DATA_ATTR

// RTC slow memory: sobrevive ao deep sleep, é apagada por corte total de energia.
// Vale lembrar que só POD pode morar aqui — nada de String ou ponteiros.
RTC_DATA_ATTR static BitaxeStatus g_snapshot;
RTC_DATA_ATTR static uint32_t g_snapshotEpoch = 0;
RTC_DATA_ATTR static uint32_t g_magic = 0;

#define SNAPSHOT_MAGIC 0x8175A1E2

bool snapshot_load(BitaxeStatus &out)
{
    if (g_magic != SNAPSHOT_MAGIC) {
        return false;
    }
    out = g_snapshot;
    return true;
}

void snapshot_save(const BitaxeStatus &s, uint32_t epoch)
{
    g_snapshot = s;
    g_snapshotEpoch = epoch;
    g_magic = SNAPSHOT_MAGIC;
}

uint32_t snapshot_age(uint32_t nowEpoch)
{
    if (g_magic != SNAPSHOT_MAGIC || nowEpoch <= g_snapshotEpoch) {
        return 0;
    }
    return nowEpoch - g_snapshotEpoch;
}
```

O `g_magic` é o que distingue "nunca gravado" de "gravado com valores zerados" — sem ele, a RTC memory recém-inicializada passaria por um snapshot legítimo cheio de zeros.

- [ ] **Step 3: Confirmar que compila para o alvo**

```powershell
& $pio run -e t-watch-2020-v1
```

Esperado: compila sem erro. O módulo ainda não é chamado por ninguém.

- [ ] **Step 4: Commit**

```bash
git add twatch/src/snapshot_store.h twatch/src/snapshot_store.cpp
git commit -m "feat: cache last Bitaxe status in RTC slow memory"
```

---

### Task 7: `net` e `bitaxe_fetch`

**Files:**
- Create: `twatch/src/net.h`, `twatch/src/net.cpp`
- Create: `twatch/src/bitaxe_fetch.h`, `twatch/src/bitaxe_fetch.cpp`
- Delete: `twatch/src/bitaxe_client.h`, `twatch/src/bitaxe_client.cpp`
- Modify: `twatch/include/config.h`

**Interfaces:**
- Consumes: `bitaxe_parse` (Task 3), `BitaxeStatus` (Task 2)
- Produces: `bool net_connect(uint32_t timeoutMs)`, `void net_disconnect()`, `bool net_sync_time()`, `enum FetchResult`, `FetchResult bitaxe_fetch(BitaxeStatus &out, int &httpCode)` — usados pela Task 10

- [ ] **Step 1: Ajustar a configuração**

Em `twatch/include/config.h`, substitua `POLL_INTERVAL_MS` (o polling contínuo deixou de existir) por:

```cpp
#define WIFI_CONNECT_TIMEOUT_MS 8000
#define HTTP_TIMEOUT_MS 4000
#define BATTERY_MIN_PERCENT 15
#define NTP_SERVER "pool.ntp.org"
```

Mantenha `WIFI_SSID`, `WIFI_PASS` e `BITAXE_HOST`. Remova `TEMP_WARN_C`: a temperatura saiu do painel.

- [ ] **Step 2: Implementar o `net`**

`twatch/src/net.h`:

```cpp
#pragma once

#include <stdint.h>

bool net_connect(uint32_t timeoutMs);
void net_disconnect();

// Acerta o RTC por NTP. Só faz sentido com o WiFi já conectado.
bool net_sync_time();
```

`twatch/src/net.cpp`:

```cpp
#include "net.h"

#include <WiFi.h>
#include <time.h>

#include "config.h"

bool net_connect(uint32_t timeoutMs)
{
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);

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

bool net_sync_time()
{
    configTime(0, 0, NTP_SERVER);
    struct tm t;
    return getLocalTime(&t, 5000);
}
```

- [ ] **Step 3: Implementar o `bitaxe_fetch`**

`twatch/src/bitaxe_fetch.h`:

```cpp
#pragma once

#include "bitaxe_status.h"

enum FetchResult
{
    FETCH_OK,
    FETCH_NO_NETWORK,   // WiFi não conectou
    FETCH_NO_RESPONSE,  // timeout ou erro de transporte
    FETCH_BAD_STATUS,   // HTTP != 200 (401 = falta o header Origin)
    FETCH_BAD_JSON,     // resposta ilegível; o cache é preservado
};

// Presume WiFi já conectado. Não mexe em `out` a menos que devolva FETCH_OK.
FetchResult bitaxe_fetch(BitaxeStatus &out, int &httpCode);
```

`twatch/src/bitaxe_fetch.cpp`:

```cpp
#include "bitaxe_fetch.h"

#include <HTTPClient.h>
#include <WiFi.h>

#include "bitaxe_parse.h"
#include "config.h"

FetchResult bitaxe_fetch(BitaxeStatus &out, int &httpCode)
{
    httpCode = 0;

    if (WiFi.status() != WL_CONNECTED) {
        return FETCH_NO_NETWORK;
    }

    const String url = String("http://") + BITAXE_HOST + "/api/system/info";

    HTTPClient http;
    http.setConnectTimeout(HTTP_TIMEOUT_MS);
    http.setTimeout(HTTP_TIMEOUT_MS);

    if (!http.begin(url)) {
        return FETCH_NO_RESPONSE;
    }

    // O ESP-Miner valida private-network CORS: sem o header Origin a resposta
    // é 401, mesmo com IP correto e o dispositivo saudável.
    http.addHeader("Origin", String("http://") + BITAXE_HOST);

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

- [ ] **Step 4: Remover o cliente antigo**

```bash
git rm twatch/src/bitaxe_client.h twatch/src/bitaxe_client.cpp
```

- [ ] **Step 5: Confirmar que compila**

```powershell
& $pio run -e t-watch-2020-v1
```

Esperado: erros em `main.cpp`, que ainda inclui `bitaxe_client.h`. Deixe-os para a Task 10 apenas se estiver executando as tarefas em sequência; caso contrário, comente temporariamente o corpo do `main.cpp` para validar este módulo isoladamente.

- [ ] **Step 6: Commit**

```bash
git add twatch/src/net.h twatch/src/net.cpp twatch/src/bitaxe_fetch.h twatch/src/bitaxe_fetch.cpp twatch/include/config.h
git commit -m "feat: split network layer into net and bitaxe_fetch"
```

---

### Task 8: `clock_view`

**Files:**
- Create: `twatch/src/clock_view.h`, `twatch/src/clock_view.cpp`

**Interfaces:**
- Produces: `void clock_view_draw(TTGOClass *watch, bool timeIsValid)` — usada pela Task 10

- [ ] **Step 1: Implementar**

`twatch/src/clock_view.h`:

```cpp
#pragma once

#include <LilyGoWatch.h>

// Desenha o mostrador. Sem rede: a hora vem do PCF8563, que conta sozinho
// durante o deep sleep.
void clock_view_draw(TTGOClass *watch, bool timeIsValid);
```

`twatch/src/clock_view.cpp`:

```cpp
#include "clock_view.h"

void clock_view_draw(TTGOClass *watch, bool timeIsValid)
{
    TFT_eSPI *tft = watch->tft;
    tft->fillScreen(TFT_BLACK);
    tft->setTextDatum(MC_DATUM);

    if (!timeIsValid) {
        // O PCF8563 não descobre a hora sozinho, e a única janela de rede
        // é o painel. Até visitá-lo uma vez, não há hora para mostrar.
        tft->setTextColor(TFT_DARKGREY, TFT_BLACK);
        tft->setTextFont(6);
        tft->drawString("--:--", 120, 110);
        tft->setTextFont(2);
        tft->drawString("abra o painel", 120, 160);
        tft->drawString("para acertar a hora", 120, 180);
        return;
    }

    RTC_Date now = watch->rtc->getDateTime();

    char buf[8];
    snprintf(buf, sizeof(buf), "%02d:%02d", now.hour, now.minute);
    tft->setTextColor(TFT_WHITE, TFT_BLACK);
    tft->setTextFont(7);
    tft->drawString(buf, 120, 110);

    snprintf(buf, sizeof(buf), "%02d/%02d", now.day, now.month);
    tft->setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft->setTextFont(2);
    tft->drawString(buf, 120, 160);
}
```

- [ ] **Step 2: Confirmar que compila**

```powershell
& $pio run -e t-watch-2020-v1
```

- [ ] **Step 3: Commit**

```bash
git add twatch/src/clock_view.h twatch/src/clock_view.cpp
git commit -m "feat: add clock face view"
```

---

### Task 9: `panel_view`

**Files:**
- Create: `twatch/src/panel_view.h`, `twatch/src/panel_view.cpp`

**Interfaces:**
- Consumes: `BitaxeStatus` (Task 2), `metrics_*` (Task 4), `FetchResult` (Task 7)
- Produces: `void panel_view_draw(TTGOClass *, const BitaxeStatus *, uint32_t ageSeconds, const char *note)` — usada pela Task 10

`panel_view` recebe uma struct e uma idade e desenha. Não sabe se o dado veio da rede ou do cache — é isso que faz o caso "offline com dado velho" percorrer o mesmo caminho do caso normal.

- [ ] **Step 1: Implementar**

`twatch/src/panel_view.h`:

```cpp
#pragma once

#include <LilyGoWatch.h>

#include "bitaxe_status.h"

// `status` nulo significa cache vazio: desenha traços, não erro.
// `note` é um aviso curto no rodapé ("sem rede", "401") ou nullptr.
void panel_view_draw(TTGOClass *watch, const BitaxeStatus *status,
                     uint32_t ageSeconds, const char *note);
```

`twatch/src/panel_view.cpp`:

```cpp
#include "panel_view.h"

#include <stdio.h>

#include "metrics.h"

void panel_view_draw(TTGOClass *watch, const BitaxeStatus *status,
                     uint32_t ageSeconds, const char *note)
{
    TFT_eSPI *tft = watch->tft;
    tft->fillScreen(TFT_BLACK);

    if (!status) {
        tft->setTextDatum(MC_DATUM);
        tft->setTextColor(TFT_DARKGREY, TFT_BLACK);
        tft->setTextFont(4);
        tft->drawString("---", 120, 100);
        tft->setTextFont(2);
        tft->drawString("sem dados ainda", 120, 140);
        if (note) {
            tft->drawString(note, 120, 165);
        }
        return;
    }

    const Freshness fresh = metrics_freshness(ageSeconds);
    // Acima de 10 min os números continuam legíveis, mas param de parecer atuais.
    const uint16_t primary = fresh == FRESHNESS_STALE ? TFT_DARKGREY : TFT_WHITE;
    const uint16_t accent = fresh == FRESHNESS_STALE ? TFT_DARKGREY : TFT_GREENYELLOW;

    char buf[32];

    // Cabeçalho: hostname e idade do dado
    tft->setTextDatum(TL_DATUM);
    tft->setTextFont(2);
    tft->setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft->drawString(status->hostname[0] ? status->hostname : "bitaxe", 8, 6);

    if (fresh != FRESHNESS_NOW) {
        snprintf(buf, sizeof(buf), "ha %lu min", (unsigned long) (ageSeconds / 60));
        tft->setTextDatum(TR_DATUM);
        tft->drawString(buf, 232, 6);
    }

    // Hashrate, grande
    metrics_format_hashrate(status->hashRate10m, buf, sizeof(buf));
    tft->setTextDatum(MC_DATUM);
    tft->setTextColor(accent, TFT_BLACK);
    tft->setTextFont(6);
    tft->drawString(buf, 120, 56);

    // J/TH em destaque
    float jth = 0.0f;
    tft->setTextFont(4);
    tft->setTextColor(primary, TFT_BLACK);
    if (metrics_efficiency(*status, jth)) {
        snprintf(buf, sizeof(buf), "%.1f J/TH", jth);
    } else {
        snprintf(buf, sizeof(buf), "-- J/TH");
    }
    tft->drawString(buf, 120, 100);

    // Potência à esquerda, frequência e tensão à direita
    tft->setTextDatum(TL_DATUM);
    tft->setTextFont(2);
    snprintf(buf, sizeof(buf), "%.1f W", status->power);
    tft->drawString(buf, 12, 132);

    tft->setTextDatum(TR_DATUM);
    snprintf(buf, sizeof(buf), "%.0f MHz  %.0f mV", status->frequency,
             status->coreVoltageActual);
    tft->drawString(buf, 228, 132);

    // Rodapé: bestDiff e shares aceitos
    tft->setTextDatum(TL_DATUM);
    tft->setTextColor(TFT_CYAN, TFT_BLACK);
    snprintf(buf, sizeof(buf), "best %s", status->bestDiff[0] ? status->bestDiff : "-");
    tft->drawString(buf, 12, 170);

    tft->setTextDatum(TR_DATUM);
    snprintf(buf, sizeof(buf), "%lu ok", (unsigned long) status->sharesAccepted);
    tft->drawString(buf, 228, 170);

    if (note) {
        tft->setTextDatum(BC_DATUM);
        tft->setTextColor(TFT_ORANGE, TFT_BLACK);
        tft->drawString(note, 120, 234);
    }
}
```

- [ ] **Step 2: Confirmar que compila**

```powershell
& $pio run -e t-watch-2020-v1
```

- [ ] **Step 3: Commit**

```bash
git add twatch/src/panel_view.h twatch/src/panel_view.cpp
git commit -m "feat: add Bitaxe panel view driven by status and age"
```

---

### Task 10: `main.cpp` — junta tudo e dorme

**Files:**
- Modify: `twatch/src/main.cpp` (reescrita completa)

**Interfaces:**
- Consumes: tudo das Tasks 2 a 9

- [ ] **Step 1: Reescrever o `main.cpp`**

```cpp
// Painel Bitaxe no TTGO T-Watch 2020 V1.
//
// Duas telas, WiFi só no painel, deep sleep entre consultas. O despertar
// reexecuta o setup(): só a RTC memory atravessa, e é por isso que o
// snapshot_store existe.

#include <Arduino.h>
#include <LilyGoWatch.h>
#include <time.h>

#include "bitaxe_fetch.h"
#include "clock_view.h"
#include "config.h"
#include "net.h"
#include "panel_view.h"
#include "snapshot_store.h"
#include "state_machine.h"

static TTGOClass *watch = nullptr;
static AppState state = STATE_RELOGIO;
static uint32_t lastInteraction = 0;

RTC_DATA_ATTR static bool g_timeIsValid = false;

// Converte a leitura do PCF8563 em epoch Unix. Não assuma que RTC_Date expõe
// unixtime(): confirme no header da biblioteca antes de simplificar isto.
static uint32_t nowEpoch()
{
    RTC_Date d = watch->rtc->getDateTime();
    struct tm t = {};
    t.tm_year = d.year - 1900;
    t.tm_mon = d.month - 1;
    t.tm_mday = d.day;
    t.tm_hour = d.hour;
    t.tm_min = d.minute;
    t.tm_sec = d.second;
    t.tm_isdst = -1;
    return (uint32_t) mktime(&t);
}

static const char *fetchNote(FetchResult r, int httpCode)
{
    static char buf[24];
    switch (r) {
    case FETCH_OK: return nullptr;
    case FETCH_NO_NETWORK: return "sem rede";
    case FETCH_NO_RESPONSE: return "sem resposta";
    case FETCH_BAD_JSON: return "resposta invalida";
    case FETCH_BAD_STATUS:
        snprintf(buf, sizeof(buf), "HTTP %d", httpCode);
        return buf;
    }
    return nullptr;
}

static void enterPanel()
{
    BitaxeStatus cached;
    const bool hasCache = snapshot_load(cached);

    // Pinta o cache antes de qualquer coisa de rede: é isso que troca uma
    // espera de 3 s por um número na tela com a idade explícita.
    panel_view_draw(watch, hasCache ? &cached : nullptr,
                    hasCache ? snapshot_age(nowEpoch()) : 0, nullptr);

    const int battery = watch->power->getBattPercentage();
    if (battery >= 0 && battery < BATTERY_MIN_PERCENT) {
        // Conectar é de longe a operação mais cara; preserva o relógio.
        panel_view_draw(watch, hasCache ? &cached : nullptr,
                        hasCache ? snapshot_age(nowEpoch()) : 0, "bateria baixa");
        return;
    }

    if (!net_connect(WIFI_CONNECT_TIMEOUT_MS)) {
        panel_view_draw(watch, hasCache ? &cached : nullptr,
                        hasCache ? snapshot_age(nowEpoch()) : 0, "sem rede");
        net_disconnect();
        return;
    }

    if (!g_timeIsValid && net_sync_time()) {
        struct tm t;
        if (getLocalTime(&t, 1000)) {
            watch->rtc->setDateTime(t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
                                    t.tm_hour, t.tm_min, t.tm_sec);
            g_timeIsValid = true;
        }
    }

    BitaxeStatus fresh;
    int httpCode = 0;
    const FetchResult r = bitaxe_fetch(fresh, httpCode);
    net_disconnect();

    if (r == FETCH_OK) {
        snapshot_save(fresh, nowEpoch());
        panel_view_draw(watch, &fresh, 0, nullptr);
    } else {
        panel_view_draw(watch, hasCache ? &cached : nullptr,
                        hasCache ? snapshot_age(nowEpoch()) : 0,
                        fetchNote(r, httpCode));
    }
}

static void goToSleep()
{
    watch->closeBL();
    watch->displaySleep();

    // Duas fontes de despertar, conforme a spec: o toque e o botão PEK.
    // ext0 aceita um único pino, então o par vai em ext1 com máscara.
    // Ambos os IRQs são ativos em nível baixo, daí ALL_LOW.
    const uint64_t mask = (1ULL << TOUCH_INT) | (1ULL << AXP202_INT);
    esp_sleep_enable_ext1_wakeup(mask, ESP_EXT1_WAKEUP_ALL_LOW);

    esp_deep_sleep_start();
}

void setup()
{
    Serial.begin(115200);

    watch = TTGOClass::getWatch();
    watch->begin();
    watch->openBL();

    // Sem habilitar o ADC1 do AXP202 a leitura de bateria não vale nada.
    watch->power->adc1Enable(AXP202_BATT_VOL_ADC1 | AXP202_BATT_CUR_ADC1, true);

    state = state_next(STATE_DEEP_SLEEP, EVENT_WAKE, 0);
    clock_view_draw(watch, g_timeIsValid);
    lastInteraction = millis();
}

void loop()
{
    int16_t tx = 0, ty = 0;
    const bool touched = watch->getTouch(tx, ty);

    if (touched) {
        while (watch->getTouch(tx, ty)) {
            delay(20);
        }
        lastInteraction = millis();
        state = state_next(state, EVENT_TOUCH, 0);

        if (state == STATE_PAINEL) {
            enterPanel();
        } else {
            clock_view_draw(watch, g_timeIsValid);
        }
        lastInteraction = millis();
        return;
    }

    state = state_next(state, EVENT_TICK, millis() - lastInteraction);
    if (state == STATE_DEEP_SLEEP) {
        goToSleep();
    }

    delay(50);
}
```

- [ ] **Step 2: Compilar**

```powershell
& $pio run -e t-watch-2020-v1
```

Três nomes precisam ser confirmados no header do board, em
`.pio/libdeps/t-watch-2020-v1/TTGO_TWatch_Library/src/board/` — é ele que define os
pinos do T-Watch 2020 V1:

- `TOUCH_INT` — pino de interrupção do touch
- `AXP202_INT` — pino de interrupção do PMU (pode aparecer como `AXP202_INT_PIN`)
- `RTC_Date` — confirme se os campos são `year`/`month`/`day`/`hour`/`minute`/`second`

Ambos os pinos de IRQ precisam estar no domínio RTC do ESP32 para servirem de fonte
de despertar em `ext1`. Se algum não estiver, o despertar terá de ficar só no que
estiver — registre qual e siga; isso não bloqueia o resto.

- [ ] **Step 3: Rodar toda a suíte nativa para garantir que nada regrediu**

```powershell
& $pio test -e t-watch-2020-v1
```

Esperado: 18 testes PASS (2 + 5 + 5 + 6).

- [ ] **Step 4: Commit**

```bash
git add twatch/src/main.cpp
git commit -m "feat: wire two-screen app with deep sleep and cached panel"
```

---

### Task 11: Verificação no aparelho

Os dois números que a spec assumiu e não mediu. **Se o consumo em deep sleep inviabilizar a autonomia de vários dias, a arquitetura precisa voltar à mesa** — este é o passo que decide isso.

**Files:**
- Modify: `twatch/README.md` (registrar as medições)

- [ ] **Step 1: Gravar no aparelho**

```powershell
& $pio run -e t-watch-2020-v1 -t upload --upload-port COM5
& $pio device monitor -p COM5 -b 115200
```

- [ ] **Step 2: Confirmar a PSRAM**

Adicione temporariamente ao fim do `setup()`:

```cpp
Serial.printf("PSRAM: %s, %u bytes\n", psramFound() ? "sim" : "nao",
              (unsigned) ESP.getPsramSize());
```

Se responder "nao", **remova `-DBOARD_HAS_PSRAM` do `platformio.ini`** e recompile.

- [ ] **Step 3: Medir o tempo até a tela acender**

Registre `millis()` no início do `setup()` e logo após `clock_view_draw`. Compare com os ~400 ms estimados na spec.

- [ ] **Step 4: Medir a corrente em deep sleep**

Com o relógio dormindo e desconectado do USB, meça a corrente na bateria. Calcule a autonomia com a capacidade real da célula do aparelho.

- [ ] **Step 5: Verificar o comportamento manualmente**

- toque acorda e mostra o relógio; 5 s depois volta a dormir
- toque no relógio abre o painel: valores do cache aparecem antes dos novos
- com o Bitaxe desligado, o painel mantém os valores antigos e a idade cresce
- primeiro uso após apagar a flash: relógio mostra `--:--` até visitar o painel

- [ ] **Step 6: Registrar as medições e commitar**

Substitua no `README.md` as estimativas pelos números reais.

```bash
git add twatch/README.md twatch/platformio.ini
git commit -m "docs: record measured deep sleep draw, boot time and PSRAM result"
```
