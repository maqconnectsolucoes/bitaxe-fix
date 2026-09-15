# Painel Bitaxe no T-Watch — design

Data: 2026-08-05
Branch: `T-watch`
Alvo: TTGO T-Watch 2020 V1 (ESP32-D0WDQ6-V3, 16 MB flash, PMU AXP202, tela 240x240)

## Problema

O relógio deve mostrar o estado do minerador Bitaxe numa olhada rápida, no pulso,
sem comprometer a autonomia. O código que existe hoje em `twatch/` faz polling a
cada 5 s com a tela sempre acesa e sem nenhum modo de sono — a bateria não passaria
de algumas horas.

## Decisões de uso (definidas com o usuário)

| Decisão | Escolha |
|---|---|
| Modo de uso | no pulso, consulta ocasional |
| Vigilância em segundo plano | **não** — busca só quando o usuário olha |
| Função de relógio | sim; hora primeiro, dados do minerador depois |
| Dados de interesse | progresso/sorte e eficiência |
| Abordagem | duas telas (relógio + painel) com cache que sobrevive ao sono |

Consequência direta: **não há alerta de overheat nem vibração.** O firmware do
Bitaxe tem corte térmico próprio; vigiar isso no pulso custaria manter o WiFi vivo,
que é justamente o que inviabiliza a autonomia.

## 1. Arquitetura e ciclo de vida

Máquina de estados com uma regra central: **WiFi só existe no estado do painel.**

```
DEEP_SLEEP ──(toque / botão PEK)──> RELOGIO
RELOGIO ──(toque)──> PAINEL ──(toque)──> RELOGIO
RELOGIO ──(5 s sem toque)──> DEEP_SLEEP
PAINEL ──(15 s sem toque)──> DEEP_SLEEP
```

- **DEEP_SLEEP** — CPU parada, tela apagada, rádio desligado. Sobrevivem o RTC
  PCF8563 e a fonte de despertar. O AXP202 corta os trilhos desnecessários.
- **RELOGIO** — desenha a hora lida do PCF8563 por I²C. Sem rede. Aparece em
  milissegundos e custa pouco além do backlight.
- **PAINEL** — desenha imediatamente o último estado conhecido (da RTC slow memory)
  com a idade dele; só então liga o WiFi e busca. Ao chegar a resposta, reescreve os
  números no lugar e zera a idade. Se falhar, o dado antigo permanece e a idade
  continua correndo.

### Persistência entre sonos

O despertar do deep sleep no ESP32 **reexecuta o `setup()`**: o programa recomeça do
zero e só a RTC memory atravessa. Por isso o cache é obrigatório, não um luxo.

Usa-se `RTC_DATA_ATTR` (8 KB de RTC slow memory) para guardar o último
`BitaxeStatus` e o instante em que ele chegou. É memória, não flash: escrita
ilimitada, sem desgaste. Se a bateria zerar de vez, o cache se perde — o que é
correto, pois àquela altura estaria velho demais para ter valor.

### Fonte de despertar

`ext0` no pino de interrupção do touch, mais o botão PEK do AXP202.

O giro de pulso pelo BMA423 fica **fora desta versão**: exige manter o acelerômetro
alimentado e calibrar limiar de inclinação, e dispara sozinho ao dirigir, gesticular
ou dormir — cada falso despertar acende a tela e consome bateria. Pode ser
acrescentado depois, com medição, se o toque se mostrar incômodo.

## 2. Componentes

Sete unidades. As quatro primeiras não conhecem rede; as três últimas não conhecem tela.

| Unidade | Responsabilidade | Depende de |
|---|---|---|
| `app` | máquina de estados, timeouts, decide quem desenha | todas |
| `clock_view` | desenha o mostrador; lê hora do PCF8563 | `tft`, `rtc` |
| `panel_view` | desenha o painel a partir de um `BitaxeStatus` + idade | `tft` |
| `snapshot_store` | guarda/recupera o último `BitaxeStatus` e seu instante | `RTC_DATA_ATTR` |
| `net` | liga WiFi, conecta com timeout, desliga | driver WiFi |
| `bitaxe_parse` | JSON → `BitaxeStatus`; **pura**, sem I/O | ArduinoJson |
| `bitaxe_fetch` | HTTP GET e entrega o corpo a `bitaxe_parse` | `net`, `bitaxe_parse` |

`panel_view` recebe uma struct e uma idade, e desenha. Não sabe se aquilo veio da
rede ou do cache — o que a torna testável isoladamente e faz o caso "offline com
dado velho" percorrer o mesmo caminho do caso normal, sem ramificação especial.

`snapshot_store` expõe `load()`, `save(status)` e `ageSeconds()`. A idade sai da
diferença entre o relógio atual e o instante gravado, ambos do RTC, que continua
contando durante o sono.

### Poda no `bitaxe_client` existente

De quinze campos para sete: `hashRate_10m`, `power`, `frequency`,
`coreVoltageActual`, `bestDiff`, `sharesAccepted` e `hostname`.

Saem: `temp`, `vrTemp`, `uptimeSeconds`, `fanrpm`, `sharesRejected`,
`expectedHashrate`, `hashRate` instantâneo e `overheat_mode` — este último junto com
o alerta descartado. Menos campos encolhem o filtro do ArduinoJson e a struct que vai
para a RTC memory, que tem só 8 KB.

**Fica** do código atual: o header `Origin` (sem ele a API devolve 401) e a
convenção de booleanos como 0/1 numérico.

## 3. Fluxo de dados e tela

Linha do tempo ao entrar no painel:

| Momento | O que acontece |
|---|---|
| 0 ms | toque acorda a CPU |
| ~400 ms | init do AXP202 e do display; tela acesa |
| ~450 ms | painel desenhado **do cache**, com a idade |
| ~500 ms | WiFi liga e começa a conectar |
| 2–3 s | resposta chega; números reescritos; idade zera |
| +15 s sem toque | WiFi desliga, AXP202 corta trilhos, deep sleep |

Os ~400 ms de init são **estimativa, não medição** (ver Questões em aberto).

### Layout do painel

1. **Hashrate de 10 min** — topo, tipografia grande
2. **J/TH em destaque** — `power ÷ (hashRate_10m ÷ 1000)`, pois a API entrega Gh/s
   e potência em watts
3. **Faixa do meio** — potência em W à esquerda; frequência e tensão de core à
   direita (`490 MHz · 1150 mV`)
4. **Rodapé** — `bestDiff` e shares aceitos
5. **Canto** — idade do dado: oculta quando fresco, visível acima de 1 min; acima de
   10 min os números ficam acinzentados, legíveis mas sem parecer atuais

O mostrador do relógio é independente e não compartilha layout com o painel.

## 4. Tratamento de erro

Princípio: **a tela nunca fica vazia e nunca mente sobre a idade do que mostra.**
Toda falha degrada para o cache com a idade correndo.

| Falha | Comportamento |
|---|---|
| WiFi não conecta em 8 s | mantém o cache, marca "sem rede", desliga o rádio, dorme no prazo normal |
| Bitaxe não responde / timeout | idem, marcando "sem resposta" |
| HTTP ≠ 200 | mostra o código; `401` aponta para o header `Origin` |
| JSON inválido ou campo ausente | descarta a resposta inteira e preserva o cache — meio dado é pior que dado velho |
| Cache vazio (primeiro uso ou bateria zerada) | traços no lugar dos números e "sem dados ainda"; não é erro |
| `hashRate_10m` < 1 Gh/s | J/TH vira traço — evita divisão por zero, e nessa situação a eficiência não significa nada |
| Bateria < 15% | painel recusa ligar o WiFi e diz por quê; preserva o relógio funcionando |

### Hora do relógio

O PCF8563 pode nunca ter sido acertado e não descobre a hora sozinho. A única janela
de rede é o painel, então a sincronização NTP acontece ali, oportunisticamente,
sempre que o WiFi subir.

Consequência assumida: **um relógio que nunca visitou o painel não sabe a hora.** No
primeiro uso mostra traços e orienta a abrir o painel uma vez.

## 5. Testes

Refinamento que habilita o resto: dividir o cliente em `bitaxe_parse(const char *json,
BitaxeStatus &)`, pura, e `bitaxeFetch()`, que faz HTTP e chama a primeira. Só a
segunda depende do `HTTPClient`.

Com isso, um ambiente `env:native` no `platformio.ini` roda Unity no PC, sem gravar:

- **`bitaxe_parse`** — resposta real capturada do aparelho; JSON truncado; campo
  ausente; `overheat_mode` como `0`/`1`; número onde se espera string
- **J/TH** — cálculo correto e o caso `hashRate = 0` devolvendo "sem valor"
- **Idade** — fresco, acima de 1 min, acima de 10 min, limiar de acinzentar
- **Máquina de estados** — cada transição e cada timeout da seção 1
- **Formatação** — Gh/s virando Th/s na fronteira dos 1000

Alimentar `bitaxe_parse` com respostas gravadas do aparelho real faz o teste cobrir o
formato verdadeiro da API, não um inventado.

### Verificação no aparelho (não automatizável)

Desenho na tela, deep sleep e consumo. Lista curta de conferência manual.

## Questões em aberto

Dois números foram **assumidos e não medidos**. Ambos entram como tarefa de medição:

1. **Corrente em deep sleep.** O AXP202 mantém trilhos ligados por padrão, e placas
   T-Watch costumam ficar bem acima do 10 µA teórico do ESP32. A autonomia de
   "vários dias" depende disso. **Se o consumo real inviabilizar essa autonomia, a
   premissa da seção 1 cai e o desenho volta à mesa.**
2. **Tempo de init até a tela acender** (~400 ms estimados). Dominado pela
   inicialização de display e PMU; varia com a biblioteca.

Um terceiro item, herdado da sessão de análise do backup: **a presença de PSRAM neste
board nunca foi confirmada.** Não é bloqueante para este design — nada aqui precisa
de PSRAM — mas o `platformio.ini` declara `-DBOARD_HAS_PSRAM`, que deve ser removido
se `psramFound()` responder falso.

## Fora de escopo

- Alerta de overheat e vibração (descartados por decisão de uso)
- Giro de pulso pelo BMA423 (adiável, exige medição)
- Histórico e sparklines: o modo "busca só quando olho" não gera série contínua; um
  gráfico sairia esburacado e enganoso
- Configuração pela tela: SSID e IP seguem em `config.h`, definidos em tempo de build
