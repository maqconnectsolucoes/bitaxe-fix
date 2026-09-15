# T-Watch Fase 1 — camada visual e bateria na tela

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Trocar o desenho direto e piscante das três telas por um sprite em PSRAM com tipografia vetorial e paleta única, e mostrar a carga da bateria no relógio.

**Architecture:** Um `TFT_eSprite` de 240x240 alocado uma vez na PSRAM concentra todo o desenho; as views recebem uma referência `TFT_eSPI&` e não sabem se pintam no sprite ou no display. A lógica de bateria (percentual, faixa, segmentos) nasce pura em `lib/bitaxe_core/`, testável no alvo pelo Unity.

**Tech Stack:** PlatformIO 6.1.19, `espressif32@6.9.0` (Arduino core esp32 2.0.17), `TTGO_TWatch_Library` 1.4.2, TFT_eSPI embutido, Unity.

## Global Constraints

- Alvo: TTGO T-Watch 2020 V1, ESP32-D0WDQ6-V3, PMU **AXP202** (não AXP2101).
- Tela 240x240. `SCREEN_W`/`SCREEN_H` já existem em `lib/bitaxe_core/ui_layout.h`.
- Comentários e textos de UI em pt-br **sem acentos** — a fonte do display não os desenha. Comentários de código podem ter acentos.
- Estilo C: 4 espaços, 132 colunas, chaves de função em linha própria (`.clang-format` da raiz do repo).
- Testes rodam **no alvo**, com o relógio na USB em `COM5`. O PlatformIO só compila `lib/` nas suítes — nada de lógica testável em `src/`.
- Paleta: exatamente os tokens de `src/theme.h`. Nenhuma cor literal `TFT_*` fora desse arquivo.
- `git commit` ao fim de cada tarefa, sempre com `Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>`.

---

### Task 1: Lógica de bateria (pura, TDD)

**Files:**
- Create: `twatch/lib/bitaxe_core/battery.h`
- Create: `twatch/lib/bitaxe_core/battery.cpp`
- Test: `twatch/test/test_battery/test_battery.cpp`

**Interfaces:**
- Consumes: nada.
- Produces: `int battery_percent(int axpPercent, uint16_t millivolts)`, `BatteryLevel battery_level(int percent, uint8_t minPct)`, `int battery_segments(int percent)`, constantes `BATTERY_UNKNOWN` (-1) e `BATTERY_SEGMENTS` (6), enum `BatteryLevel { BATTERY_LEVEL_UNKNOWN, BATTERY_LEVEL_LOW, BATTERY_LEVEL_MID, BATTERY_LEVEL_HIGH }`.

- [ ] **Step 1: Escrever o header**

`twatch/lib/bitaxe_core/battery.h`:

```c
#pragma once

#include <stdint.h>

// Percentual desconhecido. Existe porque o AXP202 tem dois modos de falhar e
// nenhum deles devolve um numero utilizavel — ver battery_percent.
#define BATTERY_UNKNOWN (-1)

// Segmentos desenhados dentro da pilha.
#define BATTERY_SEGMENTS 6

// Faixa da estimativa por tensao. Celula de litio de uma serie.
#define BATTERY_MV_EMPTY 3300
#define BATTERY_MV_FULL 4200

enum BatteryLevel
{
    BATTERY_LEVEL_UNKNOWN,
    BATTERY_LEVEL_LOW,   // abaixo do minimo configurado
    BATTERY_LEVEL_MID,   // do minimo ate 50%
    BATTERY_LEVEL_HIGH,  // acima de 50%
};

// `axpPercent` e o retorno cru de getBattPercentage(); `millivolts` o de
// getBattVoltage(), ou 0 quando indisponivel.
int battery_percent(int axpPercent, uint16_t millivolts);

BatteryLevel battery_level(int percent, uint8_t minPct);

// 0..BATTERY_SEGMENTS. Arredonda para cima: 1% ja acende um segmento.
int battery_segments(int percent);
```

- [ ] **Step 2: Escrever a suíte de testes**

`twatch/test/test_battery/test_battery.cpp`:

```c
#include <Arduino.h>
#include <unity.h>

#include "battery.h"

void test_percentual_do_axp_vence_a_tensao(void)
{
    // Com o registrador valido, a tensao nem e consultada.
    TEST_ASSERT_EQUAL(77, battery_percent(77, 3500));
}

void test_axp_zero_cai_na_tensao(void)
{
    // getBattPercentage devolve 0 tanto para "sem bateria" quanto para bit de
    // validade baixo. Zero nunca e tratado como carga real.
    TEST_ASSERT_EQUAL(50, battery_percent(0, 3750));
}

void test_axp_negativo_cai_na_tensao(void)
{
    TEST_ASSERT_EQUAL(50, battery_percent(-1, 3750));
}

void test_axp_acima_de_cem_cai_na_tensao(void)
{
    TEST_ASSERT_EQUAL(100, battery_percent(120, 4200));
}

void test_sem_axp_e_sem_tensao_e_desconhecido(void)
{
    TEST_ASSERT_EQUAL(BATTERY_UNKNOWN, battery_percent(0, 0));
}

void test_tensao_abaixo_do_piso_satura_em_zero(void)
{
    TEST_ASSERT_EQUAL(0, battery_percent(0, 3100));
}

void test_tensao_acima_do_teto_satura_em_cem(void)
{
    TEST_ASSERT_EQUAL(100, battery_percent(0, 4300));
}

void test_nivel_abaixo_do_minimo_e_baixo(void)
{
    TEST_ASSERT_EQUAL(BATTERY_LEVEL_LOW, battery_level(9, 15));
}

void test_nivel_no_minimo_ja_e_medio(void)
{
    // O corte do painel usa "< minimo"; a cor tem que concordar com ele.
    TEST_ASSERT_EQUAL(BATTERY_LEVEL_MID, battery_level(15, 15));
}

void test_nivel_em_cinquenta_ainda_e_medio(void)
{
    TEST_ASSERT_EQUAL(BATTERY_LEVEL_MID, battery_level(50, 15));
}

void test_nivel_acima_de_cinquenta_e_alto(void)
{
    TEST_ASSERT_EQUAL(BATTERY_LEVEL_HIGH, battery_level(51, 15));
}

void test_nivel_desconhecido_nao_vira_baixo(void)
{
    // Pintar de vermelho o que nao se sabe seria mentira com cara de alarme.
    TEST_ASSERT_EQUAL(BATTERY_LEVEL_UNKNOWN, battery_level(BATTERY_UNKNOWN, 15));
}

void test_segmentos_zero_por_cento(void)
{
    TEST_ASSERT_EQUAL(0, battery_segments(0));
}

void test_segmentos_um_por_cento_acende_um(void)
{
    TEST_ASSERT_EQUAL(1, battery_segments(1));
}

void test_segmentos_cem_por_cento_acende_todos(void)
{
    TEST_ASSERT_EQUAL(BATTERY_SEGMENTS, battery_segments(100));
}

void test_segmentos_desconhecido_nao_acende_nada(void)
{
    TEST_ASSERT_EQUAL(0, battery_segments(BATTERY_UNKNOWN));
}

void setUp(void) {}
void tearDown(void) {}

// Testes no alvo usam setup()/loop(), nao main().
void setup()
{
    delay(2000);
    UNITY_BEGIN();
    RUN_TEST(test_percentual_do_axp_vence_a_tensao);
    RUN_TEST(test_axp_zero_cai_na_tensao);
    RUN_TEST(test_axp_negativo_cai_na_tensao);
    RUN_TEST(test_axp_acima_de_cem_cai_na_tensao);
    RUN_TEST(test_sem_axp_e_sem_tensao_e_desconhecido);
    RUN_TEST(test_tensao_abaixo_do_piso_satura_em_zero);
    RUN_TEST(test_tensao_acima_do_teto_satura_em_cem);
    RUN_TEST(test_nivel_abaixo_do_minimo_e_baixo);
    RUN_TEST(test_nivel_no_minimo_ja_e_medio);
    RUN_TEST(test_nivel_em_cinquenta_ainda_e_medio);
    RUN_TEST(test_nivel_acima_de_cinquenta_e_alto);
    RUN_TEST(test_nivel_desconhecido_nao_vira_baixo);
    RUN_TEST(test_segmentos_zero_por_cento);
    RUN_TEST(test_segmentos_um_por_cento_acende_um);
    RUN_TEST(test_segmentos_cem_por_cento_acende_todos);
    RUN_TEST(test_segmentos_desconhecido_nao_acende_nada);
    UNITY_END();
}

void loop() {}
```

- [ ] **Step 3: Rodar e ver falhar**

```bash
cd twatch
pio test --upload-port COM5 -f test_battery
```

Esperado: falha de compilação — `battery.cpp` não existe, nenhum símbolo definido.

- [ ] **Step 4: Implementar**

`twatch/lib/bitaxe_core/battery.cpp`:

```c
#include "battery.h"

int battery_percent(int axpPercent, uint16_t millivolts)
{
    // O registrador AXP202_BATT_PERCENTAGE devolve 0 para "sem bateria" e para
    // bit de validade baixo, e negativo quando o chip nao inicializou. So a
    // faixa 1..100 e informacao.
    if (axpPercent > 0 && axpPercent <= 100) {
        return axpPercent;
    }

    if (millivolts == 0) {
        return BATTERY_UNKNOWN;
    }
    if (millivolts <= BATTERY_MV_EMPTY) {
        return 0;
    }
    if (millivolts >= BATTERY_MV_FULL) {
        return 100;
    }

    // Reta entre os dois extremos. Grosseira de proposito: o numero decide
    // "da para ligar o radio?", nao prediz autonomia.
    const int32_t span = BATTERY_MV_FULL - BATTERY_MV_EMPTY;
    return (int) (((int32_t) (millivolts - BATTERY_MV_EMPTY) * 100) / span);
}

BatteryLevel battery_level(int percent, uint8_t minPct)
{
    if (percent < 0 || percent > 100) {
        return BATTERY_LEVEL_UNKNOWN;
    }
    if (percent < (int) minPct) {
        return BATTERY_LEVEL_LOW;
    }
    if (percent <= 50) {
        return BATTERY_LEVEL_MID;
    }
    return BATTERY_LEVEL_HIGH;
}

int battery_segments(int percent)
{
    if (percent <= 0) {
        return 0;
    }
    if (percent >= 100) {
        return BATTERY_SEGMENTS;
    }
    // Teto da divisao: 1% precisa acender um segmento, senao a pilha fica vazia
    // com o aparelho ainda ligado.
    return (percent * BATTERY_SEGMENTS + 99) / 100;
}
```

- [ ] **Step 5: Rodar e ver passar**

```bash
cd twatch
pio test --upload-port COM5 -f test_battery
```

Esperado: 16 testes, todos PASS.

- [ ] **Step 6: Commit**

```bash
git add twatch/lib/bitaxe_core/battery.h twatch/lib/bitaxe_core/battery.cpp twatch/test/test_battery/
git commit -m "feat: add battery percent, level and segment logic"
```

---

### Task 2: Tema e camada de desenho

**Files:**
- Create: `twatch/src/theme.h`
- Create: `twatch/src/screen.h`
- Create: `twatch/src/screen.cpp`
- Modify: `twatch/src/main.cpp` (`setup()`)

**Interfaces:**
- Consumes: `SCREEN_W`/`SCREEN_H` de `ui_layout.h`.
- Produces: `bool screen_begin(TTGOClass *watch)`, `TFT_eSPI &screen_canvas(void)`, `void screen_flush(void)`, `void screen_brightness(uint8_t percent, bool fade)`; tokens `THEME_BG`, `THEME_PRIMARY`, `THEME_MUTED`, `THEME_STALE`, `THEME_ACCENT`, `THEME_GOOD`, `THEME_WARN`, `THEME_DANGER`; fontes `FONT_HERO`, `FONT_VALUE`, `FONT_LABEL`; grade `GRID_LEFT`, `GRID_RIGHT`.

- [ ] **Step 1: Escrever o tema**

`twatch/src/theme.h`. As FreeFonts já vêm incluídas por `TFT_eSPI.h:577` (`Fonts/GFXFF/GFXFF.h`), então basta `LilyGoWatch.h` para usá-las:

```c
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

// Grade compartilhada pelas tres telas: rotulo a esquerda, valor a direita.
#define GRID_LEFT 16
#define GRID_RIGHT 224
```

- [ ] **Step 2: Escrever o header da camada de desenho**

`twatch/src/screen.h`:

```c
#pragma once

#include <LilyGoWatch.h>

// Aloca o sprite de 240x240 na PSRAM. Idempotente. false significa que o
// sprite nao coube: o desenho cai direto no display e volta a piscar, mas
// o firmware continua funcionando.
bool screen_begin(TTGOClass *watch);

// TFT_eSprite herda de TFT_eSPI e sobrescreve os primitivos como virtuais
// (TFT_eSPI.h:815), entao o mesmo codigo de desenho serve aos dois caminhos.
TFT_eSPI &screen_canvas(void);

// Empurra o sprite para o display. No-op quando nao ha sprite.
void screen_flush(void);

// Brilho em porcentagem. O fade evita o corte seco entre telas.
void screen_brightness(uint8_t percent, bool fade);
```

- [ ] **Step 3: Implementar a camada de desenho**

`twatch/src/screen.cpp`:

```c
#include "screen.h"

#include "ui_layout.h"

static TTGOClass *g_watch = nullptr;
static TFT_eSprite *g_sprite = nullptr;
static uint8_t g_brightness = 100;

bool screen_begin(TTGOClass *watch)
{
    g_watch = watch;
    if (g_sprite != nullptr) {
        return true;
    }

    g_sprite = new TFT_eSprite(watch->tft);
    g_sprite->setColorDepth(16);

    // 240*240*2 = 115200 bytes. createSprite cai em ps_calloc sozinho quando ha
    // PSRAM (TFT_eSPI.cpp:5506), entao isso nao disputa com a heap interna.
    if (g_sprite->createSprite(SCREEN_W, SCREEN_H) == nullptr) {
        delete g_sprite;
        g_sprite = nullptr;
        return false;
    }
    return true;
}

TFT_eSPI &screen_canvas(void)
{
    if (g_sprite != nullptr) {
        return *g_sprite;
    }
    return *g_watch->tft;
}

void screen_flush(void)
{
    if (g_sprite != nullptr) {
        g_sprite->pushSprite(0, 0);
    }
}

void screen_brightness(uint8_t percent, bool fade)
{
    if (percent > 100) {
        percent = 100;
    }
    const int target = (percent * 255) / 100;
    const int from = (g_brightness * 255) / 100;

    if (!fade || from == target) {
        g_watch->setBrightness((uint8_t) target);
    } else {
        // 16 degraus de 8 ms: 128 ms no total, curto o bastante para nao
        // atrasar a resposta ao toque.
        for (int i = 1; i <= 16; i++) {
            g_watch->setBrightness((uint8_t) (from + (target - from) * i / 16));
            delay(8);
        }
    }
    g_brightness = percent;
}
```

- [ ] **Step 4: Chamar no boot**

Em `twatch/src/main.cpp`, acrescentar `#include "screen.h"` ao bloco de includes e, dentro de `setup()`, logo depois de `watch->openBL();`:

```c
    // Antes de qualquer desenho: as views todas pintam no canvas que isto cria.
    if (!screen_begin(watch)) {
        Serial.println("sprite indisponivel: desenho direto no display");
    }
```

- [ ] **Step 5: Compilar e gravar**

```bash
cd twatch
pio run -t upload
pio device monitor
```

Esperado: compila, grava e **não** imprime "sprite indisponivel". As telas continuam idênticas às de hoje — nenhuma view usa o canvas ainda. Confira também que as linhas de PSRAM livre do boot caíram ~115 KB em relação ao log anterior.

- [ ] **Step 6: Commit**

```bash
git add twatch/src/theme.h twatch/src/screen.h twatch/src/screen.cpp twatch/src/main.cpp
git commit -m "feat: add PSRAM sprite canvas and theme tokens"
```

---

### Task 3: Relógio redesenhado com bateria

**Files:**
- Modify: `twatch/src/clock_view.h`
- Modify: `twatch/src/clock_view.cpp`
- Modify: `twatch/src/main.cpp`

**Interfaces:**
- Consumes: `screen_canvas()`, `screen_flush()`, tokens de `theme.h`, `battery_percent()`, `battery_level()`, `battery_segments()`, `CONFIG_STRIP_TOP_Y` de `ui_layout.h`.
- Produces: `struct ClockViewModel { bool timeIsValid; int batteryPercent; bool charging; uint8_t batteryMinPct; }` e `void clock_view_draw(TTGOClass *watch, const ClockViewModel &vm)`.

O struct existe para que a Fase 3 acrescente o contador de passos sem mexer na assinatura de novo.

- [ ] **Step 1: Reescrever o header**

`twatch/src/clock_view.h`:

```c
#pragma once

#include <LilyGoWatch.h>
#include <stdint.h>

// Struct em vez de parametros soltos: a Fase 3 acrescenta passos aqui sem
// mudar a assinatura de novo.
struct ClockViewModel
{
    bool timeIsValid;
    int batteryPercent;  // BATTERY_UNKNOWN quando nao ha leitura confiavel
    bool charging;
    uint8_t batteryMinPct;
};

void clock_view_draw(TTGOClass *watch, const ClockViewModel &vm);
```

- [ ] **Step 2: Reescrever o desenho**

`twatch/src/clock_view.cpp` inteiro:

```c
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

    snprintf(buf, sizeof(buf), "%02d/%02d", now.day, now.month);
    c.setFreeFont(FONT_LABEL);
    c.setTextColor(THEME_MUTED, THEME_BG);
    c.drawString(buf, 120, 126);

    screen_flush();
}
```

- [ ] **Step 3: Alimentar o modelo no main**

Em `twatch/src/main.cpp`, acrescentar `#include "battery.h"` aos includes e uma função auxiliar logo acima de `enterPanel()`:

```c
// Le a bateria uma vez e monta o modelo do relogio. A leitura crua do AXP202
// nao e confiavel sozinha — battery_percent aplica o fallback por tensao.
static ClockViewModel clockModel()
{
    ClockViewModel vm;
    vm.timeIsValid = g_timeIsValid;
    vm.batteryPercent = battery_percent(watch->power->getBattPercentage(),
                                        (uint16_t) watch->power->getBattVoltage());
    vm.charging = watch->power->isChargeing();
    vm.batteryMinPct = g_settings.batteryMinPct;
    return vm;
}
```

Trocar as **três** chamadas de `clock_view_draw(watch, g_timeIsValid)` por `clock_view_draw(watch, clockModel())`. Estão em `main.cpp:158` (`enterConfig()`, ramo de falha do portal), `main.cpp:234` (fim do `setup()`) e `main.cpp:270` (o `else` do bloco de toque em `loop()`).

- [ ] **Step 4: Compilar, gravar e conferir no aparelho**

```bash
cd twatch
pio run -t upload
```

Esperado: o relógio mostra hora e data em fonte suavizada, a pilha de bateria com a porcentagem abaixo da data, e **não pisca** ao redesenhar. Com o cabo USB conectado, o raio de carga aparece sobre a pilha.

- [ ] **Step 5: Commit**

```bash
git add twatch/src/clock_view.h twatch/src/clock_view.cpp twatch/src/main.cpp
git commit -m "feat: redraw clock on sprite with battery gauge"
```

---

### Task 4: Painel redesenhado

**Files:**
- Modify: `twatch/src/panel_view.cpp`

**Interfaces:**
- Consumes: `screen_canvas()`, `screen_flush()`, tokens de `theme.h`, `metrics_freshness()`, `metrics_efficiency()`, `metrics_format_hashrate_parts()`.
- Produces: assinatura inalterada — `void panel_view_draw(TTGOClass *watch, const BitaxeStatus *status, uint32_t ageSeconds, const char *note)`. A Fase 2 acrescenta os parâmetros de dispositivo.

- [ ] **Step 1: Reescrever o desenho**

`twatch/src/panel_view.cpp` inteiro. O parâmetro `watch` deixa de ser usado — o canvas vem de `screen_canvas()` — mas a assinatura fica para não mexer nos chamadores nesta tarefa:

```c
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

    c.setTextDatum(TR_DATUM);
    c.setFreeFont(FONT_VALUE);
    c.setTextColor(valueColor, THEME_BG);
    c.drawString(value, GRID_RIGHT, y - 4);
}

void panel_view_draw(TTGOClass *watch, const BitaxeStatus *status, uint32_t ageSeconds,
                     const char *note)
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
```

- [ ] **Step 2: Compilar, gravar e conferir**

```bash
cd twatch
pio run -t upload
```

Esperado: painel com hashrate em âmbar no topo, quatro linhas de grade alinhadas (rótulo à esquerda, valor à direita), sem piscar na troca cache → dado fresco. Desligue o Bitaxe e confirme que a tela inteira acinzenta quando o dado passa de 10 minutos.

- [ ] **Step 3: Commit**

```bash
git add twatch/src/panel_view.cpp
git commit -m "feat: redraw panel with typographic grid on sprite"
```

---

### Task 5: Tela de configuração redesenhada e README

**Files:**
- Modify: `twatch/src/config_view.cpp`
- Modify: `twatch/README.md`

**Interfaces:**
- Consumes: `screen_canvas()`, `screen_flush()`, tokens de `theme.h`.
- Produces: assinaturas inalteradas de `config_view_draw`, `config_view_tick`, `config_view_message`.

`config_view_tick` passava a apagar só uma faixa para não repintar a tela. Com o sprite isso deixa de fazer sentido: repintar tudo custa ~23 ms e roda uma vez por segundo. O tick guarda o último estado para poder redesenhar sozinho.

- [ ] **Step 1: Reescrever o desenho**

`twatch/src/config_view.cpp` inteiro:

```c
#include "config_view.h"

#include <stdio.h>
#include <string.h>

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
```

- [ ] **Step 2: Atualizar o README**

Em `twatch/README.md`, na seção "Estado atual", substituir a frase sobre as seis suítes pela contagem nova (sete suítes, 64 casos — 48 antigos mais 16 de bateria) e acrescentar, logo depois da seção "Configuração", uma seção nova:

```markdown
## Como as telas sao desenhadas

Todo desenho passa por um `TFT_eSprite` de 240x240 alocado na PSRAM
(`src/screen.cpp`), empurrado de uma vez por `screen_flush()`. E isso que
elimina o piscar do `fillScreen` seguido de texto.

`screen_canvas()` devolve uma referencia `TFT_eSPI&`. Funciona porque
`TFT_eSprite` herda de `TFT_eSPI` e sobrescreve os primitivos declarados
virtuais em `TFT_eSPI.h:815` — `drawPixel`, `drawChar`, `drawLine`, as duas
fast-lines e `fillRect`. Tudo o mais (`fillScreen`, `drawString`, `fillCircle`)
e construido sobre esses, entao cai no sprite sozinho. **Se algum dia um
primitivo nao-virtual for usado, ele pintara o display direto e o sprite ficara
para tras** — foi o cuidado que definiu esta arquitetura.

Se `createSprite` falhar, `screen_canvas()` devolve o display real e
`screen_flush()` vira no-op: volta a piscar, mas continua funcionando.

Cores e fontes vivem so em `src/theme.h`. Nenhuma constante `TFT_*` deve
aparecer fora de la.
```

- [ ] **Step 3: Compilar, gravar e conferir a tela de config**

```bash
cd twatch
pio run -t upload
```

Esperado: toque na engrenagem abre a tela de configuração com a mesma tipografia das outras duas, e o contador regressivo atualiza a cada segundo sem piscar nem deixar rastro de texto anterior.

- [ ] **Step 4: Rodar a bateria de testes inteira**

```bash
cd twatch
pio test --upload-port COM5
```

Esperado: sete suítes, 64 casos, todos PASS.

- [ ] **Step 5: Commit**

```bash
git add twatch/src/config_view.cpp twatch/README.md
git commit -m "feat: redraw config screen on sprite and document the canvas"
```

---

## Verificação final da fase

Antes de considerar a Fase 1 pronta, no aparelho:

1. Relógio mostra hora, data e pilha de bateria; a porcentagem bate com o que o AXP202 reporta no log serial.
2. Nenhuma das três telas pisca ao redesenhar.
3. Com o USB ligado, o raio de carga aparece.
4. Painel acinzenta por inteiro quando o dado passa de 10 minutos.
5. `pio test --upload-port COM5` fecha em 64 de 64.
6. O log de boot **não** contém "sprite indisponivel".
