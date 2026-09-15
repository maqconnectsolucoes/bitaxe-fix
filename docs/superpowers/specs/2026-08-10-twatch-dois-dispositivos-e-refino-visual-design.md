# T-Watch — segundo Bitaxe, bateria na tela e refino visual

Data: 2026-08-10
Projeto: `twatch/` (TTGO T-Watch 2020 V1, ESP32-D0WDQ6-V3)
Antecessor: `2026-08-10-config-portal-twatch-design.md`

## Problema

Três lacunas, e a terceira só apareceu quando as duas primeiras foram olhadas de perto.

**Só um Bitaxe.** O usuário tem dois — `10.1.1.154` e `10.1.1.126`, ambos
BM1370/Gamma. O relógio mostra um. O spec do portal cativo adiou explicitamente o
segundo aparelho para uma spec própria; esta é ela.

**A bateria é invisível.** `getBattPercentage()` já é lido, mas só para o corte de
segurança que impede o painel de ligar o rádio com pouca carga. O usuário nunca vê
quanto resta — justamente num aparelho cuja autonomia é a preocupação central do
projeto.

**As telas parecem protótipo.** `panel_view` mistura branco, cinza-escuro,
verde-amarelo, ciano e laranja em cinco papéis diferentes, usa quatro fontes
bitmap do TFT_eSPI e redesenha com `fillScreen(TFT_BLACK)` seguido de texto — o
que pisca de forma visível a cada atualização. O aparelho tem 4 MB de PSRAM
ociosa e a lib traz fontes vetoriais e sprites que nada disso usa.

## Objetivo

Dois Bitaxe alternáveis por toque, carga da bateria visível no relógio, e as duas
telas redesenhadas com uma linguagem visual única — sem piscar, com tipografia
vetorial e um acento de cor só. Mais quatro recursos do relógio que estavam
parados: vibração de alerta, brilho por tela, contador de passos e uma vigia
periódica que avisa se um minerador cair.

## Escopo

**Nesta spec:** tudo acima.

**Fora:** soma dos dois aparelhos numa tela de total, histórico ou gráfico de
hashrate, nomes amigáveis por dispositivo (o `hostname` já vem da API),
infravermelho e áudio (nada no projeto os justifica), e wake-on-tilt — este
último por um motivo de hardware explicado abaixo.

## Decisões e o porquê

**Alternar por toque, não dividir a tela.** Dois painéis em 240x240 espremem cada
aparelho em 120 px de altura e obrigam a derrubar o número herói para uma fonte
que não se lê de relance no pulso. O valor do relógio é o olhar de dois segundos;
um dispositivo por vez preserva isso.

**Ciclo de três toques.** `relógio → dev 1 → dev 2 → relógio`. Nenhum alvo novo
para acertar, nenhum gesto invisível, e com um único host configurado o ciclo tem
dois passos — exatamente o comportamento de hoje. Uma faixa de "voltar" no rodapé
do painel colidiria visualmente com a engrenagem do relógio, que ocupa a mesma
região com outro significado.

**Uma conexão WiFi busca os dois.** Conectar é de longe a operação mais cara do
ciclo. Buscar sob demanda economizaria uma requisição HTTP e pagaria por ela com
uma reconexão de rádio inteira a cada alternância — pior em bateria e três
segundos de espera onde hoje há uma troca instantânea. Alternar entre dispositivos
não toca no rádio: é redesenho a partir do snapshot.

**Dois slots de snapshot com magic próprio.** A RTC memory é escassa, mas
`BitaxeStatus` ocupa cerca de 70 bytes (quatro `float`, um `uint32_t` e dois
`char[24]`); dois cabem sem aperto. Magic por slot, não global,
para que um aparelho offline não invalide o cache do outro.

**`bitaxeHost2` opcional, e recusado se igual ao host 1.** Campo vazio significa
"um dispositivo" e todo o resto se comporta como hoje. Dois hosts iguais é erro
de digitação que produziria duas telas idênticas sem nenhum sintoma — mais barato
recusar no formulário.

**Bateria só no relógio.** O painel é dedicado a números de mineração; enfiar
carga do relógio ali competiria com o dado que motivou abrir a tela. O relógio é
onde o olhar cai primeiro e onde sobra espaço.

**Percentual do AXP202 com fallback por tensão.** `getBattPercentage()` devolve 0
tanto para "sem bateria conectada" quanto quando o bit de validade do registrador
`AXP202_BATT_PERCENTAGE` está baixo, e negativo quando o chip não inicializou —
conferido em `axp20x.cpp:1176`. Esse registrador é reconhecidamente pouco
confiável em parte dos AXP202. Regra: valor maior que zero vale; senão, estimativa
linear por `getBattVoltage()` entre 3300 e 4200 mV; se nem isso, a tela mostra
`--%` com a pilha cinza. Mentir sobre carga num aparelho a bateria é pior que
admitir que não se sabe.

**Sprite full-screen na PSRAM.** `TFT_eSprite::callocSprite` já cai em `ps_calloc`
quando `psramFound()` (`TFT_eSPI.cpp:5506`), então os 115 KB de 240x240x16bpp vão
para a PSRAM sozinhos, sem tocar na heap interna. Desenhar no sprite e empurrar
de uma vez elimina o piscar e é o que torna o estilo tipográfico viável — ele
depende de redesenhar a tela inteira a cada troca.

**Tipografia GFXFF, não VLW.** `SMOOTH_FONT` e `LOAD_GFXFF` estão ativos na lib, e
`FreeSans`/`FreeSansBold` de 9 a 24pt já vêm compiladas. As fontes VLW
anti-aliased seriam mais bonitas ainda, mas exigem gerar o arquivo por fora do
projeto — fricção e risco que não se paga agora. **Custo aceito:** o maior GFXFF
tem ~34 px de caixa alta contra os 48 px da Font7 atual, então hora e hashrate
ficam menores, porém muito melhor desenhados. Reavaliar VLW só se o tamanho
incomodar no pulso.

**Vigia periódica por timer, não por alarme do PCF8563.** `esp_sleep_enable_timer_wakeup`
é uma fonte de despertar independente que convive com o `ext0` do toque e o `ext1`
do botão PEK sem disputar modo — o alarme do RTC precisaria de um terceiro pino e
cairia no mesmo conflito descrito a seguir.

**Wake-on-tilt fica de fora.** O BMA423 interrompe em GPIO39 ativo em nível
**alto**. O ESP32 clássico tem `ext0` (um pino) e `ext1` (um modo único para toda
a máscara: `ALL_LOW` ou `ANY_HIGH`). Hoje `ext0` é o toque em nível baixo e `ext1`
é o PEK em `ALL_LOW`. Um terceiro pino ativo em alto não cabe sem sacrificar o
toque ou o botão. O recurso é real e bonito; o preço não vale.

## Arquitetura

### Arquivos novos

| Arquivo | Papel | Testável |
|---|---|---|
| `lib/bitaxe_core/panel_cycle.h/.cpp` | `panel_cycle_next()` — o ciclo relógio/dev1/dev2 | sim |
| `lib/bitaxe_core/battery.h/.cpp` | percentual, faixa de cor e segmentos da pilha | sim |
| `lib/bitaxe_core/watchdog.h/.cpp` | decisão de alertar e de rodar a vigia | sim |
| `src/theme.h` | os tokens de cor e as escolhas de fonte, num lugar só | não |
| `src/screen.h/.cpp` | o sprite da PSRAM, o flush e o brilho com fade | não |

### Arquivos alterados

| Arquivo | Mudança |
|---|---|
| `lib/bitaxe_core/settings.h/.cpp` | `bitaxeHost2`, brilhos, intervalo da vigia; validação e formulário |
| `lib/bitaxe_core/state_machine.h/.cpp` | evento `EVENT_TOUCH_NEXT` |
| `src/snapshot_store.h/.cpp` | dois slots indexados |
| `src/settings_store.cpp` | quatro chaves NVS novas |
| `src/panel_view.cpp` | redesenho tipográfico, indicador de dispositivo |
| `src/clock_view.cpp` | redesenho tipográfico, pilha de bateria, passos |
| `src/config_view.cpp` | mesma linguagem visual das outras duas |
| `src/config_portal.cpp` | campos novos no formulário |
| `src/main.cpp` | ciclo, busca dupla, vibração, brilho, despertar por timer |
| `include/config.h.example` | sementes novas |

### Contratos

```c
// panel_cycle.h — PANEL_CYCLE_CLOCK significa "sair do painel"
#define PANEL_CYCLE_CLOCK (-1)
int panel_cycle_next(int current, int deviceCount);
```

`current == PANEL_CYCLE_CLOCK` devolve `0`. O último dispositivo devolve
`PANEL_CYCLE_CLOCK`. `deviceCount` fora de `1..2`, ou `current` fora da faixa,
devolve `PANEL_CYCLE_CLOCK` — a saída segura é sempre o relógio.

```c
// battery.h
#define BATTERY_UNKNOWN (-1)
#define BATTERY_SEGMENTS 6

enum BatteryLevel { BATTERY_LEVEL_UNKNOWN, BATTERY_LEVEL_LOW, BATTERY_LEVEL_MID, BATTERY_LEVEL_HIGH };

int battery_percent(int axpPercent, uint16_t millivolts);
BatteryLevel battery_level(int percent, uint8_t minPct);
int battery_segments(int percent);
```

`battery_percent` aplica a regra do fallback: `axpPercent > 0` vale direto;
senão, `millivolts` entre 3300 e 4200 mapeado linearmente com clamp; `0` mV
devolve `BATTERY_UNKNOWN`. `battery_level` devolve `LOW` abaixo de `minPct`,
`MID` até 50%, `HIGH` acima; percentual desconhecido devolve `UNKNOWN`.

```c
// watchdog.h
struct WatchdogInputs
{
    uint8_t deviceCount;
    bool online[2];
    float hashRate[2];
};

bool watchdog_should_alert(const WatchdogInputs &in);
bool watchdog_should_run(uint32_t intervalMinutes, int batteryPercent, uint8_t minPct);
```

Alerta quando qualquer dispositivo configurado não respondeu ou está com hashrate
zerado. A vigia roda quando o intervalo é diferente de zero e a bateria não é
conhecidamente baixa — bateria desconhecida não bloqueia, mesma regra que o painel
já aplica hoje.

```c
// snapshot_store.h
#define SNAPSHOT_SLOTS 2
bool snapshot_load(uint8_t slot, BitaxeStatus &out);
void snapshot_save(uint8_t slot, const BitaxeStatus &s, uint32_t epoch);
uint32_t snapshot_age(uint8_t slot, uint32_t nowEpoch);
void snapshot_clear(void);
```

`snapshot_clear` limpa os dois: é chamado quando o portal grava, e qualquer
mudança de host ou de fuso invalida ambos.

```c
// screen.h — TFT_eSprite herda de TFT_eSPI, então o mesmo código de desenho
// serve para os dois caminhos e o fallback sai de graça.
bool screen_begin(TTGOClass *watch);
TFT_eSPI &screen_canvas(void);
void screen_flush(void);
void screen_brightness(uint8_t percent, bool fade);
```

Se `createSprite` falhar, `screen_canvas()` devolve o TFT real e `screen_flush()`
vira no-op: o firmware volta a piscar, mas continua funcionando. Degradar é
preferível a abortar num aparelho de pulso.

### Settings — campos novos

| Campo | Chave NVS | Faixa | Erro |
|---|---|---|---|
| `bitaxeHost2` | `host2` | vazio, ou IPv4 diferente de `bitaxeHost` | `SETTINGS_ERR_HOST2` |
| `brightnessClock` | `brtclk` | 10..100 % | `SETTINGS_ERR_BRIGHTNESS` |
| `brightnessPanel` | `brtpan` | 10..100 % | `SETTINGS_ERR_BRIGHTNESS` |
| `watchIntervalMin` | `watchmin` | 0 (desligado) ou 5..240 min | `SETTINGS_ERR_WATCH` |

Sementes no `config.h.example`: `BITAXE_HOST2 ""`, `BRIGHTNESS_CLOCK 40`,
`BRIGHTNESS_PANEL 100`, `WATCH_INTERVAL_MIN 0`. A vigia nasce desligada — é o
único recurso que gasta bateria sem o usuário pedir, então precisa ser escolha
explícita.

## Fluxos

### Entrar no painel (vindo do relógio)

1. `panel_cycle_next(PANEL_CYCLE_CLOCK, count)` devolve o dispositivo 0.
2. Desenha o slot 0 a partir do cache, com a idade explícita — antes de qualquer rede.
3. Corte de bateria: abaixo do mínimo, nota "bateria baixa" e para aqui.
4. Uma conexão WiFi. Sincroniza a hora se ainda não for válida.
5. Busca o dispositivo 0; se há dois configurados, busca o 1 na mesma conexão.
6. Desconecta. Grava os snapshots que vieram bem; os que falharam mantêm o cache.
7. `watchdog_should_alert` verdadeiro dispara uma vibração curta.
8. Redesenha o dispositivo 0 com o dado fresco.

### Alternar

`EVENT_TOUCH_NEXT` mantém `STATE_PAINEL` e reinicia o contador de ociosidade. O
desenho sai do snapshot — nenhuma rede, nenhuma espera. Quando
`panel_cycle_next` devolve `PANEL_CYCLE_CLOCK`, o `main` emite `EVENT_TOUCH` e a
máquina de estados volta ao relógio pelo caminho que já existe.

### Vigia periódica

Ao dormir, se `watchdog_should_run`, arma `esp_sleep_enable_timer_wakeup` junto
com as duas fontes atuais. No `setup()`, `esp_sleep_get_wakeup_cause() ==
ESP_SLEEP_WAKEUP_TIMER` entra no caminho silencioso: **não** chama `openBL()`,
conecta, busca os dois, grava os snapshots, vibra duas vezes se houver alerta e
volta a dormir sem desenhar nada. O usuário percebe pela vibração, ou pelos dados
já frescos quando abrir o painel depois.

**Custo declarado:** cada ciclo mantém o rádio ligado por volta de 5 a 8 segundos.
Em 15 minutos de intervalo, são ~96 ciclos por dia. Por isso nasce desligada e se
auto-suspende com bateria baixa.

## Linguagem visual

Quatro tokens de cor em `theme.h`, e nada fora deles: fundo preto, texto primário
branco, texto secundário cinza, acento âmbar `#F7931A` (`0xF483` em RGB565).
Vermelho e amarelo aparecem apenas na pilha de bateria e em notas de erro — são
sinais, não decoração. Dado velho (acima de 10 minutos) continua sendo acinzentado
por inteiro, regra que já existe e que passa a valer também no cabeçalho e no
indicador de dispositivo.

Hierarquia por peso e tamanho, sem molduras: `FreeSansBold24` no número herói,
`FreeSansBold12` nos valores da grade, `FreeSans9` nos rótulos em maiúsculas. Os
rótulos ficam alinhados à esquerda e os valores à direita, na mesma grade nas três
telas.

```
  PAINEL                      RELOGIO

  bitaxe-1            ●○         14:32
                                 10/08
  1362
  Gh/s · 15.2 J/TH           ▮▮▮▮▮▯ 78%   ⇢ 4.812

  POTENCIA       20.7 W      ────────────────────
  CLOCK         665 MHz                ⚙
  CORE         1200 mV
  BEST    1.2T · 4821 ok
```

A pilha de bateria e a engrenagem são desenhadas antes do ramo de hora inválida —
o mesmo cuidado que o código atual já toma com a engrenagem, pelo mesmo motivo: é
a tela em que o usuário ainda não configurou nada, e é onde a carga mais importa.

## Testes

Suítes novas, no mesmo padrão Unity das seis atuais, todas rodando no alvo:

| Suíte | Cobre |
|---|---|
| `test_cycle` | ciclo com 1 e com 2 dispositivos, entrada inválida, retorno ao relógio |
| `test_battery` | percentual do AXP, fallback por tensão, clamp, faixas de cor, segmentos |
| `test_watchdog` | alerta por offline e por hashrate zero, suspensão por bateria, intervalo zero |

Estendidas: `test_settings` (host2 vazio, inválido, igual ao host 1; brilhos e
intervalo fora de faixa) e `test_state` (`EVENT_TOUCH_NEXT` mantendo o painel).

Desenho não é testado por unidade — é validado no aparelho, com o relógio na USB.

**Por que `snapshot_store` não ganha suíte.** O PlatformIO só compila `lib/` nas
suítes de teste; `src/` fica de fora. Ligar `test_build_src` arrastaria o
`main.cpp` para dentro de cada suíte, e o `setup()/loop()` dele colidiria com o
`setup()/loop()` do Unity — quebrando as seis suítes que já passam. É por isso
que toda lógica testável nasce em `lib/bitaxe_core/`, e por que os dois slots de
snapshot são verificados no aparelho: abrir o painel com um dispositivo
desligado e confirmar que o outro mantém cache e idade próprios.

## Entregas

**Fase 1 — o que se vê.** `theme.h`, `screen.h/.cpp`, as três telas redesenhadas,
bateria no relógio, `battery.h/.cpp` e `test_battery`. `screen_brightness` nasce
aqui junto do resto do módulo, mas só passa a ser chamada na fase 3 — o brilho
segue fixo até lá. Ao fim da fase o relógio grava e roda com uma cara nova, ainda
com um Bitaxe só.

**Fase 2 — o segundo aparelho.** `settings` com `host2`, `snapshot_store` de dois
slots, `panel_cycle`, `EVENT_TOUCH_NEXT`, busca dupla numa conexão, indicador
`●○`, campo no portal, e as suítes `test_cycle`, `test_snapshot` e `test_settings`
estendida.

**Fase 3 — os recursos parados.** Vibração de alerta, brilho por tela com fade,
passos do BMA423, vigia periódica, `watchdog.h/.cpp` e `test_watchdog`, mais os
campos correspondentes no portal.

Cada fase termina em firmware gravável e testável no aparelho — não há estado
intermediário quebrado entre elas.

## Riscos

**O sprite pode não caber junto com o WiFi.** 115 KB em PSRAM não disputam com a
heap interna, mas o stack WiFi tem alocações próprias em PSRAM. Mitigado pelo
fallback de `screen_begin`, e observável no log de heap que o `setup()` já imprime.

**A estimativa linear por tensão é grosseira.** A curva real de uma célula de
lítio não é reta, e o erro no meio da faixa chega a alguns pontos percentuais. É
aceitável porque o número serve para decidir "dá para abrir o painel?", não para
prever autonomia — e só entra em cena quando o registrador do AXP falha.

**A vigia periódica pode surpreender na bateria.** Nasce desligada, se
auto-suspende com carga baixa, e o intervalo mínimo é de 5 minutos justamente para
impedir uma configuração que drene o aparelho em uma tarde.
