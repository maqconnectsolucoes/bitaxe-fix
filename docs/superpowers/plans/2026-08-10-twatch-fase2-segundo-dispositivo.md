# T-Watch Fase 2 — segundo Bitaxe

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Dois Bitaxe no relógio, alternáveis por toque num ciclo `relógio → dev 1 → dev 2 → relógio`, buscados numa única conexão WiFi.

**Architecture:** O segundo host é um campo opcional das `Settings` — vazio mantém o comportamento de um dispositivo só. O cache na RTC memory vira dois slots indexados com magic próprio. O ciclo de navegação é uma função pura em `lib/bitaxe_core/`, e a máquina de estados ganha um evento que mantém o painel aceso ao alternar.

**Tech Stack:** PlatformIO 6.1.19, `espressif32@6.9.0`, `TTGO_TWatch_Library` 1.4.2, Unity.

**Depende de:** Fase 1 concluída (`docs/superpowers/plans/2026-08-10-twatch-fase1-camada-visual.md`).

## Global Constraints

- Alvo: TTGO T-Watch 2020 V1, PMU **AXP202**.
- Textos de UI em pt-br **sem acentos**.
- Estilo C: 4 espaços, 132 colunas, chaves de função em linha própria.
- Testes rodam **no alvo** em `COM5`. O PlatformIO só compila `lib/` nas suítes — lógica testável nunca vai para `src/`.
- Nenhuma constante `TFT_*` fora de `src/theme.h`.
- Host do Bitaxe é sempre **IPv4 explícito**: `bitaxe.local` resolve para IPv6 e a API devolve 401.
- `git commit` ao fim de cada tarefa, com `Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>`.

---

### Task 1: Segundo host nas Settings

**Files:**
- Modify: `twatch/lib/bitaxe_core/settings.h`
- Modify: `twatch/lib/bitaxe_core/settings.cpp`
- Modify: `twatch/src/settings_store.cpp`
- Modify: `twatch/include/config.h.example`
- Modify: `twatch/include/config.h` (local, fora do git)
- Test: `twatch/test/test_settings/test_settings.cpp`

**Interfaces:**
- Consumes: `valid_ipv4()` e `copy_string()`, ambas estáticas já existentes em `settings.cpp`.
- Produces: campo `char bitaxeHost2[SETTINGS_HOST_MAX + 1]` em `Settings`, campo `const char *host2` em `SettingsForm`, `SETTINGS_ERR_HOST2`, e `uint8_t settings_device_count(const Settings &s)`.

- [ ] **Step 1: Escrever os testes novos**

Em `twatch/test/test_settings/test_settings.cpp`, acrescentar `strcpy(s.bitaxeHost2, "");` ao final de `baseline()` (antes do `return`) e `f.host2 = "";` ao final de `baselineForm()`. Depois acrescentar os casos:

```c
void test_host2_vazio_aprova(void)
{
    // Vazio e o caso normal: um dispositivo so.
    Settings s = baseline();
    TEST_ASSERT_EQUAL(SETTINGS_OK, settings_validate(s));
}

void test_host2_valido_aprova(void)
{
    Settings s = baseline();
    strcpy(s.bitaxeHost2, "10.1.1.126");
    TEST_ASSERT_EQUAL(SETTINGS_OK, settings_validate(s));
}

void test_host2_igual_ao_primeiro_reprova(void)
{
    // Erro de digitacao que produziria duas telas identicas sem sintoma nenhum.
    Settings s = baseline();
    strcpy(s.bitaxeHost2, s.bitaxeHost);
    TEST_ASSERT_EQUAL(SETTINGS_ERR_HOST2, settings_validate(s));
}

void test_host2_com_nome_reprova(void)
{
    Settings s = baseline();
    strcpy(s.bitaxeHost2, "bitaxe.local");
    TEST_ASSERT_EQUAL(SETTINGS_ERR_HOST2, settings_validate(s));
}

void test_contagem_de_dispositivos_sem_segundo_host(void)
{
    TEST_ASSERT_EQUAL(1, settings_device_count(baseline()));
}

void test_contagem_de_dispositivos_com_segundo_host(void)
{
    Settings s = baseline();
    strcpy(s.bitaxeHost2, "10.1.1.126");
    TEST_ASSERT_EQUAL(2, settings_device_count(s));
}

void test_form_host2_vazio_desliga_o_segundo(void)
{
    // Diferente da senha: aqui vazio significa "desligar", nao "manter".
    Settings current = baseline();
    strcpy(current.bitaxeHost2, "10.1.1.126");

    SettingsForm f = baselineForm();
    f.host2 = "";

    Settings out = {};
    TEST_ASSERT_EQUAL(SETTINGS_OK, settings_apply_form(current, f, out));
    TEST_ASSERT_EQUAL_STRING("", out.bitaxeHost2);
}

void test_form_host2_nulo_desliga_o_segundo(void)
{
    Settings current = baseline();
    strcpy(current.bitaxeHost2, "10.1.1.126");

    SettingsForm f = baselineForm();
    f.host2 = NULL;

    Settings out = {};
    TEST_ASSERT_EQUAL(SETTINGS_OK, settings_apply_form(current, f, out));
    TEST_ASSERT_EQUAL_STRING("", out.bitaxeHost2);
}

void test_form_host2_igual_ao_primeiro_reprova(void)
{
    SettingsForm f = baselineForm();
    f.host = "10.0.0.7";
    f.host2 = "10.0.0.7";

    Settings out = {};
    TEST_ASSERT_EQUAL(SETTINGS_ERR_HOST2, settings_apply_form(baseline(), f, out));
}

void test_equal_detecta_diferenca_no_host2(void)
{
    Settings a = baseline();
    Settings b = baseline();
    strcpy(b.bitaxeHost2, "10.1.1.126");
    TEST_ASSERT_FALSE(settings_equal(a, b));
}
```

Registrar todos no `setup()`, depois da linha `RUN_TEST(test_host_incompleto_reprova);`:

```c
    RUN_TEST(test_host2_vazio_aprova);
    RUN_TEST(test_host2_valido_aprova);
    RUN_TEST(test_host2_igual_ao_primeiro_reprova);
    RUN_TEST(test_host2_com_nome_reprova);
    RUN_TEST(test_contagem_de_dispositivos_sem_segundo_host);
    RUN_TEST(test_contagem_de_dispositivos_com_segundo_host);
    RUN_TEST(test_form_host2_vazio_desliga_o_segundo);
    RUN_TEST(test_form_host2_nulo_desliga_o_segundo);
    RUN_TEST(test_form_host2_igual_ao_primeiro_reprova);
    RUN_TEST(test_equal_detecta_diferenca_no_host2);
```

- [ ] **Step 2: Rodar e ver falhar**

```bash
cd twatch
pio test --upload-port COM5 -f test_settings
```

Esperado: falha de compilação — `bitaxeHost2`, `host2`, `SETTINGS_ERR_HOST2` e `settings_device_count` não existem.

- [ ] **Step 3: Estender o header**

Em `twatch/lib/bitaxe_core/settings.h`, no `struct Settings`, logo depois de `bitaxeHost`:

```c
    // Vazio significa "um dispositivo so" — nao e erro, e o caso normal.
    char bitaxeHost2[SETTINGS_HOST_MAX + 1];
```

No `struct SettingsForm`, depois de `host`:

```c
    const char *host2;  // vazio ou NULL desliga o segundo dispositivo
```

No enum `SettingsError`, depois de `SETTINGS_ERR_HOST`:

```c
    SETTINGS_ERR_HOST2,
```

E ao final do header, depois de `settings_equal`:

```c
// 1 ou 2, conforme o segundo host esteja preenchido. Existe para que a
// contagem seja decidida num lugar so.
uint8_t settings_device_count(const Settings &s);
```

- [ ] **Step 4: Implementar**

Em `twatch/lib/bitaxe_core/settings.cpp`, dentro de `settings_validate`, logo depois do bloco que valida `bitaxeHost`:

```c
    // So valida quando preenchido. Recusar igual ao primeiro pega o erro de
    // digitacao que produziria duas telas identicas sem sintoma nenhum.
    if (s.bitaxeHost2[0] != '\0') {
        if (!valid_ipv4(s.bitaxeHost2) || strcmp(s.bitaxeHost2, s.bitaxeHost) == 0) {
            return SETTINGS_ERR_HOST2;
        }
    }
```

Em `settings_error_message`, depois do caso `SETTINGS_ERR_HOST`:

```c
    case SETTINGS_ERR_HOST2:
        return "IP do segundo Bitaxe: use um IPv4 diferente do primeiro, ou deixe em branco.";
```

Em `settings_apply_form`, logo depois do bloco de `f.host`:

```c
    // Ao contrario da senha, campo vazio aqui significa "desligar o segundo
    // dispositivo" — e um endereco, nao um segredo que a pagina nao devolve.
    if (f.host2 == NULL || f.host2[0] == '\0') {
        out.bitaxeHost2[0] = '\0';
    } else if (!copy_string(out.bitaxeHost2, sizeof(out.bitaxeHost2), f.host2)) {
        return SETTINGS_ERR_HOST2;
    }
```

Em `settings_equal`, acrescentar ao encadeamento:

```c
           strcmp(a.bitaxeHost2, b.bitaxeHost2) == 0 &&
```

E ao final do arquivo:

```c
uint8_t settings_device_count(const Settings &s)
{
    return s.bitaxeHost2[0] != '\0' ? 2 : 1;
}
```

- [ ] **Step 5: Semear e persistir o campo**

Em `twatch/include/config.h.example`, depois da linha de `BITAXE_HOST`:

```c
// Segundo Bitaxe, opcional. Vazio = um dispositivo so.
#define BITAXE_HOST2 ""
```

Em `twatch/include/config.h` (arquivo local, **não** versionado), acrescentar a mesma linha com o valor real — sem ela o build quebra:

```c
#define BITAXE_HOST2 "10.1.1.126"
```

Em `twatch/src/settings_store.cpp`, dentro de `settings_load`, depois da linha de `host`:

```c
    p.getString("host2", BITAXE_HOST2).toCharArray(out.bitaxeHost2, sizeof(out.bitaxeHost2));
```

E em `settings_save`, depois do `putString("host", ...)`:

```c
    p.putString("host2", s.bitaxeHost2);
```

- [ ] **Step 6: Rodar e ver passar**

```bash
cd twatch
pio test --upload-port COM5 -f test_settings
```

Esperado: 28 testes (18 antigos + 10 novos), todos PASS.

- [ ] **Step 7: Commit**

```bash
git add twatch/lib/bitaxe_core/settings.h twatch/lib/bitaxe_core/settings.cpp \
        twatch/src/settings_store.cpp twatch/include/config.h.example \
        twatch/test/test_settings/test_settings.cpp
git commit -m "feat: add optional second Bitaxe host to settings"
```

---

### Task 2: Campo do segundo host no portal

**Files:**
- Modify: `twatch/src/config_portal.cpp`

**Interfaces:**
- Consumes: `Settings.bitaxeHost2`, `SettingsForm.host2` da Task 1.
- Produces: nada novo — só preenche o formulário.

- [ ] **Step 1: Acrescentar o campo ao formulário**

Em `sendForm`, logo depois do bloco do campo `host`:

```c
    h += "<label>IP do segundo Bitaxe (opcional)<input name=host2 maxlength=15 value='";
    h += htmlEscape(g_current.bitaxeHost2);
    h += "' placeholder='deixe vazio para um so'></label>";
```

- [ ] **Step 2: Ler o campo no save**

Em `handleSave`, acrescentar a String junto das outras (elas precisam viver até o fim da função, porque `SettingsForm` guarda ponteiros para dentro delas):

```c
    const String host2 = server->arg("host2");
```

e o ponteiro correspondente, depois de `f.host = host.c_str();`:

```c
    f.host2 = host2.c_str();
```

- [ ] **Step 3: Compilar e testar o portal no celular**

```bash
cd twatch
pio run -t upload
```

No aparelho: toque na engrenagem, conecte o celular em `T-Watch-Setup` (senha `bitaxe1234`), abra `http://192.168.4.1`.

Esperado: o campo novo aparece preenchido com o segundo IP. Salvar com um IPv4 igual ao primeiro devolve a mensagem "IP do segundo Bitaxe: use um IPv4 diferente do primeiro, ou deixe em branco." e **não** reinicia. Salvar vazio grava e reinicia.

- [ ] **Step 4: Commit**

```bash
git add twatch/src/config_portal.cpp
git commit -m "feat: expose second Bitaxe host in config portal"
```

---

### Task 3: Cache de dois slots

**Files:**
- Modify: `twatch/src/snapshot_store.h`
- Modify: `twatch/src/snapshot_store.cpp`
- Modify: `twatch/src/main.cpp`

**Interfaces:**
- Consumes: `BitaxeStatus`.
- Produces: `SNAPSHOT_SLOTS` (2), `bool snapshot_load(uint8_t slot, BitaxeStatus &out)`, `void snapshot_save(uint8_t slot, const BitaxeStatus &s, uint32_t epoch)`, `uint32_t snapshot_age(uint8_t slot, uint32_t nowEpoch)`, `void snapshot_clear(void)`.

Sem suíte Unity: `snapshot_store` vive em `src/`, que o PlatformIO não compila nas suítes. A verificação é no aparelho, no fim da Task 5.

- [ ] **Step 1: Reescrever o header**

`twatch/src/snapshot_store.h`:

```c
#pragma once

#include <stdint.h>

#include "bitaxe_status.h"

#define SNAPSHOT_SLOTS 2

// Slot fora da faixa devolve false / e ignorado, em vez de corromper memoria
// vizinha na RTC — os indices vem da contagem de dispositivos, e uma
// configuracao trocada nao pode virar escrita fora do array.
bool snapshot_load(uint8_t slot, BitaxeStatus &out);
void snapshot_save(uint8_t slot, const BitaxeStatus &s, uint32_t epoch);
uint32_t snapshot_age(uint8_t slot, uint32_t nowEpoch);

// Limpa os dois: qualquer mudanca de host ou de fuso invalida ambos.
void snapshot_clear(void);
```

- [ ] **Step 2: Reescrever a implementação**

`twatch/src/snapshot_store.cpp`:

```c
#include "snapshot_store.h"

#include <esp_attr.h>  // RTC_DATA_ATTR

// RTC slow memory: sobrevive ao deep sleep, e apagada por corte total de energia.
// Vale lembrar que so POD pode morar aqui — nada de String ou ponteiros.
RTC_DATA_ATTR static BitaxeStatus g_snapshot[SNAPSHOT_SLOTS];
RTC_DATA_ATTR static uint32_t g_snapshotEpoch[SNAPSHOT_SLOTS];

// Magic por slot, nao global: um aparelho offline nao pode invalidar o cache
// do outro.
RTC_DATA_ATTR static uint32_t g_magic[SNAPSHOT_SLOTS];

#define SNAPSHOT_MAGIC 0x8175A1E2

bool snapshot_load(uint8_t slot, BitaxeStatus &out)
{
    if (slot >= SNAPSHOT_SLOTS || g_magic[slot] != SNAPSHOT_MAGIC) {
        return false;
    }
    out = g_snapshot[slot];
    return true;
}

void snapshot_save(uint8_t slot, const BitaxeStatus &s, uint32_t epoch)
{
    if (slot >= SNAPSHOT_SLOTS) {
        return;
    }
    g_snapshot[slot] = s;
    g_snapshotEpoch[slot] = epoch;
    g_magic[slot] = SNAPSHOT_MAGIC;
}

uint32_t snapshot_age(uint8_t slot, uint32_t nowEpoch)
{
    if (slot >= SNAPSHOT_SLOTS || g_magic[slot] != SNAPSHOT_MAGIC ||
        nowEpoch <= g_snapshotEpoch[slot]) {
        return 0;
    }
    return nowEpoch - g_snapshotEpoch[slot];
}

void snapshot_clear(void)
{
    // Zerar o magic basta: e ele que snapshot_load e snapshot_age conferem.
    for (uint8_t i = 0; i < SNAPSHOT_SLOTS; i++) {
        g_magic[i] = 0;
        g_snapshotEpoch[i] = 0;
    }
}
```

- [ ] **Step 3: Ajustar os chamadores para compilar**

Em `twatch/src/main.cpp`, `cachedAge` passa a receber o slot:

```c
static uint32_t cachedAge(int slot, bool hasCache)
{
    if (!hasCache) {
        return 0;
    }
    if (!g_timeIsValid) {
        return AGE_UNKNOWN;
    }
    return snapshot_age((uint8_t) slot, nowEpoch());
}
```

E dentro de `enterPanel()`, trocar as chamadas antigas pelas indexadas — `snapshot_load(cached)` vira `snapshot_load(0, cached)`, `snapshot_save(fresh, nowEpoch())` vira `snapshot_save(0, fresh, nowEpoch())`, e cada `cachedAge(hasCache)` vira `cachedAge(0, hasCache)`. A Task 5 reescreve essa função inteira; aqui o objetivo é só voltar a compilar.

- [ ] **Step 4: Compilar e gravar**

```bash
cd twatch
pio run -t upload
```

Esperado: compila e o comportamento é idêntico ao anterior — um dispositivo, no slot 0.

- [ ] **Step 5: Commit**

```bash
git add twatch/src/snapshot_store.h twatch/src/snapshot_store.cpp twatch/src/main.cpp
git commit -m "feat: index snapshot cache by device slot"
```

---

### Task 4: Ciclo de navegação (puro, TDD)

**Files:**
- Create: `twatch/lib/bitaxe_core/panel_cycle.h`
- Create: `twatch/lib/bitaxe_core/panel_cycle.cpp`
- Test: `twatch/test/test_cycle/test_cycle.cpp`

**Interfaces:**
- Consumes: nada.
- Produces: `PANEL_CYCLE_CLOCK` (-1), `PANEL_CYCLE_MAX_DEVICES` (2), `int panel_cycle_next(int current, int deviceCount)`.

- [ ] **Step 1: Escrever o header**

`twatch/lib/bitaxe_core/panel_cycle.h`:

```c
#pragma once

// Valor de "nao esta no painel": tanto a entrada (vindo do relogio) quanto a
// saida (ciclo terminou) usam esta sentinela.
#define PANEL_CYCLE_CLOCK (-1)

// Teto de dispositivos. Casado com SNAPSHOT_SLOTS: passar disso significaria
// gravar cache em slot que nao existe.
#define PANEL_CYCLE_MAX_DEVICES 2

// Avanca o ciclo relogio -> dev 0 -> dev 1 -> relogio. Entrada invalida sai
// pelo relogio, que e sempre o estado seguro.
int panel_cycle_next(int current, int deviceCount);
```

- [ ] **Step 2: Escrever a suíte**

`twatch/test/test_cycle/test_cycle.cpp`:

```c
#include <Arduino.h>
#include <unity.h>

#include "panel_cycle.h"

void test_do_relogio_entra_no_primeiro(void)
{
    TEST_ASSERT_EQUAL(0, panel_cycle_next(PANEL_CYCLE_CLOCK, 2));
}

void test_do_primeiro_vai_para_o_segundo(void)
{
    TEST_ASSERT_EQUAL(1, panel_cycle_next(0, 2));
}

void test_do_segundo_volta_para_o_relogio(void)
{
    TEST_ASSERT_EQUAL(PANEL_CYCLE_CLOCK, panel_cycle_next(1, 2));
}

void test_com_um_dispositivo_o_ciclo_tem_dois_passos(void)
{
    // Comportamento identico ao de antes do segundo Bitaxe existir.
    TEST_ASSERT_EQUAL(0, panel_cycle_next(PANEL_CYCLE_CLOCK, 1));
    TEST_ASSERT_EQUAL(PANEL_CYCLE_CLOCK, panel_cycle_next(0, 1));
}

void test_contagem_zero_sai_pelo_relogio(void)
{
    TEST_ASSERT_EQUAL(PANEL_CYCLE_CLOCK, panel_cycle_next(PANEL_CYCLE_CLOCK, 0));
}

void test_contagem_acima_do_teto_sai_pelo_relogio(void)
{
    TEST_ASSERT_EQUAL(PANEL_CYCLE_CLOCK, panel_cycle_next(0, PANEL_CYCLE_MAX_DEVICES + 1));
}

void test_indice_alem_da_contagem_sai_pelo_relogio(void)
{
    // Acontece de verdade: o portal desliga o segundo host enquanto o painel
    // esta no dispositivo 1.
    TEST_ASSERT_EQUAL(PANEL_CYCLE_CLOCK, panel_cycle_next(1, 1));
}

void test_indice_negativo_invalido_sai_pelo_relogio(void)
{
    TEST_ASSERT_EQUAL(PANEL_CYCLE_CLOCK, panel_cycle_next(-7, 2));
}

void setUp(void) {}
void tearDown(void) {}

void setup()
{
    delay(2000);
    UNITY_BEGIN();
    RUN_TEST(test_do_relogio_entra_no_primeiro);
    RUN_TEST(test_do_primeiro_vai_para_o_segundo);
    RUN_TEST(test_do_segundo_volta_para_o_relogio);
    RUN_TEST(test_com_um_dispositivo_o_ciclo_tem_dois_passos);
    RUN_TEST(test_contagem_zero_sai_pelo_relogio);
    RUN_TEST(test_contagem_acima_do_teto_sai_pelo_relogio);
    RUN_TEST(test_indice_alem_da_contagem_sai_pelo_relogio);
    RUN_TEST(test_indice_negativo_invalido_sai_pelo_relogio);
    UNITY_END();
}

void loop() {}
```

- [ ] **Step 3: Rodar e ver falhar**

```bash
cd twatch
pio test --upload-port COM5 -f test_cycle
```

Esperado: falha de compilação — `panel_cycle.cpp` não existe.

- [ ] **Step 4: Implementar**

`twatch/lib/bitaxe_core/panel_cycle.cpp`:

```c
#include "panel_cycle.h"

int panel_cycle_next(int current, int deviceCount)
{
    if (deviceCount < 1 || deviceCount > PANEL_CYCLE_MAX_DEVICES) {
        return PANEL_CYCLE_CLOCK;
    }
    if (current == PANEL_CYCLE_CLOCK) {
        return 0;
    }
    // Indice invalido inclui o caso real de o portal ter desligado o segundo
    // host enquanto o painel estava nele.
    if (current < 0 || current >= deviceCount) {
        return PANEL_CYCLE_CLOCK;
    }

    const int next = current + 1;
    return next >= deviceCount ? PANEL_CYCLE_CLOCK : next;
}
```

- [ ] **Step 5: Rodar e ver passar**

```bash
cd twatch
pio test --upload-port COM5 -f test_cycle
```

Esperado: 8 testes, todos PASS.

- [ ] **Step 6: Commit**

```bash
git add twatch/lib/bitaxe_core/panel_cycle.h twatch/lib/bitaxe_core/panel_cycle.cpp twatch/test/test_cycle/
git commit -m "feat: add panel device cycle logic"
```

---

### Task 5: Evento de alternância na máquina de estados

**Files:**
- Modify: `twatch/lib/bitaxe_core/state_machine.h`
- Modify: `twatch/lib/bitaxe_core/state_machine.cpp`
- Test: `twatch/test/test_state/test_state.cpp`

**Interfaces:**
- Consumes: `AppState`, `AppEvent`, `IdleTimeouts`.
- Produces: `EVENT_TOUCH_NEXT` no enum `AppEvent`.

- [ ] **Step 1: Escrever os testes novos**

Em `twatch/test/test_state/test_state.cpp`. Esta suíte já tem os timeouts de fábrica num `static const IdleTimeouts T = {5000, 15000};` no topo do arquivo, e nomes de teste em inglês — siga as duas convenções:

```c
void test_switch_keeps_panel_open(void)
{
    TEST_ASSERT_EQUAL(STATE_PAINEL, state_next(STATE_PAINEL, EVENT_TOUCH_NEXT, 0, T));
}

void test_plain_touch_on_panel_still_returns_to_clock(void)
{
    // E o fim do ciclo: quem decide qual evento emitir e o main, com
    // panel_cycle_next.
    TEST_ASSERT_EQUAL(STATE_RELOGIO, state_next(STATE_PAINEL, EVENT_TOUCH, 0, T));
}

void test_switch_on_clock_opens_panel(void)
{
    // Nao deveria acontecer, mas a funcao e total: nenhum evento pode deixar a
    // maquina num estado indefinido.
    TEST_ASSERT_EQUAL(STATE_PAINEL, state_next(STATE_RELOGIO, EVENT_TOUCH_NEXT, 0, T));
}

void test_switching_does_not_block_idle_sleep(void)
{
    TEST_ASSERT_EQUAL(STATE_DEEP_SLEEP, state_next(STATE_PAINEL, EVENT_TICK, 15000, T));
}
```

Registrar os quatro no `setup()` da suíte:

```c
    RUN_TEST(test_switch_keeps_panel_open);
    RUN_TEST(test_plain_touch_on_panel_still_returns_to_clock);
    RUN_TEST(test_switch_on_clock_opens_panel);
    RUN_TEST(test_switching_does_not_block_idle_sleep);
```

- [ ] **Step 2: Rodar e ver falhar**

```bash
cd twatch
pio test --upload-port COM5 -f test_state
```

Esperado: falha de compilação — `EVENT_TOUCH_NEXT` não existe.

- [ ] **Step 3: Implementar**

Em `twatch/lib/bitaxe_core/state_machine.h`, no enum `AppEvent`, depois de `EVENT_TOUCH_CONFIG`:

```c
    EVENT_TOUCH_NEXT,    // toque no painel que avanca para o proximo Bitaxe
```

Em `twatch/lib/bitaxe_core/state_machine.cpp`, no `case STATE_PAINEL`, **antes** do teste de `EVENT_TOUCH`:

```c
        // Alternar entre dispositivos mantem o painel e reinicia a ociosidade.
        // Quem decide entre alternar e sair e o main, via panel_cycle_next.
        if (event == EVENT_TOUCH_NEXT) {
            return STATE_PAINEL;
        }
```

E no `case STATE_RELOGIO`, incluir o evento no teste que abre o painel, para a função continuar total:

```c
        if (event == EVENT_TOUCH || event == EVENT_TOUCH_NEXT) {
            return STATE_PAINEL;
        }
```

No `case STATE_CONFIG`, incluir no teste que volta ao relógio:

```c
        if (event == EVENT_TOUCH || event == EVENT_TOUCH_CONFIG || event == EVENT_TOUCH_NEXT) {
            return STATE_RELOGIO;
        }
```

- [ ] **Step 4: Rodar e ver passar**

```bash
cd twatch
pio test --upload-port COM5 -f test_state
```

Esperado: os testes antigos mais os quatro novos, todos PASS.

- [ ] **Step 5: Commit**

```bash
git add twatch/lib/bitaxe_core/state_machine.h twatch/lib/bitaxe_core/state_machine.cpp \
        twatch/test/test_state/test_state.cpp
git commit -m "feat: add panel switch event to state machine"
```

---

### Task 6: Busca dupla, ciclo e indicador na tela

**Files:**
- Modify: `twatch/src/panel_view.h`
- Modify: `twatch/src/panel_view.cpp`
- Modify: `twatch/src/main.cpp`
- Modify: `twatch/README.md`

**Interfaces:**
- Consumes: `panel_cycle_next`, `settings_device_count`, `snapshot_load/save/age` indexados, `EVENT_TOUCH_NEXT`.
- Produces: `void panel_view_draw(TTGOClass *watch, const BitaxeStatus *status, uint32_t ageSeconds, const char *note, int deviceIndex, uint8_t deviceCount)`.

- [ ] **Step 1: Estender a assinatura do painel**

Em `twatch/src/panel_view.h`, substituir a declaração por:

```c
// `deviceIndex` e `deviceCount` so servem ao indicador do topo. Com um
// dispositivo o indicador nao e desenhado, e a tela fica identica a de antes.
void panel_view_draw(TTGOClass *watch, const BitaxeStatus *status, uint32_t ageSeconds,
                     const char *note, int deviceIndex, uint8_t deviceCount);
```

- [ ] **Step 2: Desenhar o indicador**

Em `twatch/src/panel_view.cpp`, atualizar a assinatura da definição para casar com o header e acrescentar, logo depois do bloco que desenha o cabeçalho (hostname e idade):

```c
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
```

No ramo de `!status` (sem dados), o indicador não é desenhado: sem dispositivo não há o que indicar. Acrescentar `(void) deviceIndex; (void) deviceCount;` logo antes do `screen_flush()` desse ramo não é necessário — os parâmetros já são usados adiante na função.

- [ ] **Step 3: Reescrever o fluxo do painel no main**

Em `twatch/src/main.cpp`, acrescentar aos includes:

```c
#include "panel_cycle.h"
```

Logo abaixo de `static uint32_t lastCountdown = 0;`, acrescentar o estado do ciclo e as notas por dispositivo:

```c
// Dispositivo em exibicao. PANEL_CYCLE_CLOCK enquanto o painel esta fechado.
static int g_panelDevice = PANEL_CYCLE_CLOCK;

// Nota de falha por dispositivo. Precisa sobreviver a alternancia: o painel
// alterna sem rede, e a nota do dispositivo 1 nao pode aparecer no 0.
static char g_note[SNAPSHOT_SLOTS][24];
```

Substituir a função `enterPanel()` inteira por estas duas:

```c
// Desenha um dispositivo a partir do cache. Sem rede: e o caminho da
// alternancia, e tambem o primeiro quadro ao abrir o painel.
static void drawDevice(int index)
{
    BitaxeStatus cached;
    const bool hasCache = snapshot_load((uint8_t) index, cached);

    const char *note = g_note[index][0] != '\0' ? g_note[index] : nullptr;
    if (note == nullptr && hasCache && !g_timeIsValid) {
        note = "hora incerta";
    }

    panel_view_draw(watch, hasCache ? &cached : nullptr, cachedAge(index, hasCache), note, index,
                    settings_device_count(g_settings));
}

static void enterPanel()
{
    const uint8_t count = settings_device_count(g_settings);
    const char *hosts[SNAPSHOT_SLOTS] = { g_settings.bitaxeHost, g_settings.bitaxeHost2 };

    // Pinta o cache antes de qualquer coisa de rede: e isso que troca uma
    // espera de 3 s por um numero na tela com a idade explicita.
    drawDevice(0);

    // batteryNow() ja existe (main.cpp, criado no fecho da Fase 1): e o leitor
    // unico que o relogio, o painel e a config compartilham. Nao voltar a ler
    // getBattPercentage() cru aqui — foi exatamente a divergencia corrigida la.
    const int battery = batteryNow();
    if (battery >= 0 && battery < (int) g_settings.batteryMinPct) {
        // Conectar e de longe a operacao mais cara; preserva o relogio.
        for (uint8_t i = 0; i < count; i++) {
            snprintf(g_note[i], sizeof(g_note[i]), "%s", "bateria baixa");
        }
        drawDevice(0);
        return;
    }

    if (!net_connect(g_settings.wifiSsid, g_settings.wifiPass, g_settings.wifiTimeoutMs)) {
        for (uint8_t i = 0; i < count; i++) {
            snprintf(g_note[i], sizeof(g_note[i]), "%s", "sem rede");
        }
        drawDevice(0);
        net_disconnect();
        return;
    }

    if (!g_timeIsValid && net_sync_time(g_settings.tzMinutes)) {
        struct tm t;
        if (getLocalTime(&t, 1000)) {
            watch->rtc->setDateTime(t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min,
                                    t.tm_sec);
            g_timeIsValid = true;
        }
    }

    // Os dois na MESMA conexao: reconectar por dispositivo custaria outro ciclo
    // de radio inteiro, que e o gasto que domina a bateria.
    for (uint8_t i = 0; i < count; i++) {
        BitaxeStatus fresh;
        int httpCode = 0;
        const FetchResult r = bitaxe_fetch(hosts[i], g_settings.httpTimeoutMs, fresh, httpCode);

        if (r == FETCH_OK) {
            snapshot_save(i, fresh, nowEpoch());
            g_note[i][0] = '\0';
        } else {
            // O cache do dispositivo que falhou e preservado; so a nota muda.
            snprintf(g_note[i], sizeof(g_note[i]), "%s", fetchNote(r, httpCode));
        }
    }
    net_disconnect();

    drawDevice(g_panelDevice == PANEL_CYCLE_CLOCK ? 0 : g_panelDevice);
}
```

- [ ] **Step 4: Reescrever o tratamento de toque**

Em `loop()`, substituir o bloco `if (touched) { ... }` inteiro por:

```c
    if (touched) {
        // A posicao precisa ser copiada ANTES de drenar o toque: o laco abaixo
        // chama getTouch de novo e sobrescreve tx/ty, e a ultima leitura (a que
        // falha) nao tem coordenada valida.
        const int16_t px = tx;
        const int16_t py = ty;

        int16_t dx = 0, dy = 0;
        while (watch->getTouch(dx, dy)) {
            delay(20);
        }

        AppEvent ev = EVENT_TOUCH;
        int nextDevice = g_panelDevice;

        if (state == STATE_RELOGIO && clock_hit(px, py) == HIT_CONFIG) {
            ev = EVENT_TOUCH_CONFIG;
        } else if (state == STATE_PAINEL) {
            // No painel o toque avanca o ciclo; so o fim dele volta ao relogio.
            nextDevice = panel_cycle_next(g_panelDevice, settings_device_count(g_settings));
            ev = nextDevice == PANEL_CYCLE_CLOCK ? EVENT_TOUCH : EVENT_TOUCH_NEXT;
        }

        const AppState previous = state;
        state = state_next(state, ev, 0, idleTimeouts());

        if (state != STATE_CONFIG) {
            // No-op se o portal nao estiver no ar.
            config_portal_end();
        }

        if (state == STATE_PAINEL) {
            if (previous == STATE_PAINEL) {
                // Alternar e redesenho puro: os dois snapshots ja vieram na
                // mesma conexao quando o painel abriu.
                g_panelDevice = nextDevice;
                drawDevice(g_panelDevice);
            } else {
                g_panelDevice = 0;
                enterPanel();
            }
        } else if (state == STATE_CONFIG) {
            g_panelDevice = PANEL_CYCLE_CLOCK;
            enterConfig();
        } else {
            g_panelDevice = PANEL_CYCLE_CLOCK;
            clock_view_draw(watch, clockModel());
        }
        lastInteraction = millis();
        return;
    }
```

- [ ] **Step 5: Limpar o ciclo ao dormir**

Em `goToSleep()`, antes de `watch->closeBL();`:

```c
    // O despertar reexecuta o setup() e o painel comeca fechado; deixar o
    // indice velho aqui faria o primeiro toque cair no dispositivo errado.
    g_panelDevice = PANEL_CYCLE_CLOCK;
```

- [ ] **Step 6: Verificar no aparelho**

```bash
cd twatch
pio run -t upload
pio device monitor
```

Roteiro, com os dois Bitaxe ligados:

1. Toque no meio do relógio → painel do dispositivo 0, indicador `●○` no topo.
2. Toque → dispositivo 1, indicador `○●`, **instantâneo** (sem espera de rede).
3. Toque → volta ao relógio.
4. Desligue o segundo Bitaxe e abra o painel: o dispositivo 0 mostra dados frescos, o 1 mostra o cache com idade e a nota de falha. **Confirme que a nota não aparece no dispositivo 0** — é o ponto que valida os slots independentes.
5. Pelo portal, apague o segundo IP e salve. O ciclo volta a ter dois passos e o indicador some.

- [ ] **Step 7: Atualizar o README**

Em `twatch/README.md`, acrescentar depois da seção "Configuração":

```markdown
## Dois Bitaxe

O campo "IP do segundo Bitaxe" no portal e opcional. Vazio, tudo funciona como
com um aparelho so.

Com dois, o toque no painel percorre um ciclo: relogio, dispositivo 1,
dispositivo 2, relogio. Os dois circulos no topo dizem onde voce esta.

Os dois sao consultados na **mesma conexao WiFi**, quando o painel abre.
Alternar depois nao liga o radio — e redesenho do cache, instantaneo. Cada
dispositivo tem seu proprio slot na RTC memory, com idade e nota de falha
proprias: um aparelho desligado nao contamina o outro.
```

E atualizar a contagem na seção "Estado atual" para **oito suítes** — as seis originais, mais `test_battery` da Fase 1 e `test_cycle` desta. `test_settings` e `test_state` cresceram, mas não são suítes novas.

- [ ] **Step 8: Rodar a bateria inteira e commitar**

```bash
cd twatch
pio test --upload-port COM5
```

Esperado: oito suítes, todas PASS.

```bash
git add twatch/src/panel_view.h twatch/src/panel_view.cpp twatch/src/main.cpp twatch/README.md
git commit -m "feat: cycle between two Bitaxe devices fetched in one connection"
```

---

## Verificação final da fase

1. Ciclo de três toques funciona com dois hosts, e de dois toques com um só.
2. Alternar não liga o rádio: o segundo dispositivo aparece sem espera perceptível.
3. Um Bitaxe desligado mostra cache com idade e nota própria; o outro segue fresco.
4. Portal recusa segundo IP igual ao primeiro, e aceita campo vazio.
5. Salvar pelo portal limpa os dois slots (o painel volta a "sem dados ainda").
6. `pio test --upload-port COM5` fecha em oito suítes, todas verdes.
