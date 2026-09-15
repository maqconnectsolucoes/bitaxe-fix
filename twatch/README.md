# T-Watch — painel Bitaxe

Firmware para **TTGO T-Watch 2020 V1** que mostra o estado de um ou dois Bitaxe
no pulso: hashrate, eficiência, potência, clock, tensão do core e melhor share,
lidos da API REST do ESP-Miner.

## Por que este projeto existe aqui

O relógio rodava o [My-TTGO-Watch](https://github.com/sharandac/My-TTGO-Watch),
compilado no Arduino IDE com core esp32 2.0.17 em março de 2024. O fonte daquele
build se perdeu junto com a instalação do Arduino. Este projeto reinicia do zero
com o ambiente **versionado em arquivo** (`platformio.ini` + `partitions.csv`),
para que o mesmo problema não se repita.

O backup completo do firmware anterior está em
`C:\bitaxe\T-Watch\firmware-backup\` — flash inteira de 16 MB, validada por
dupla leitura com SHA256 idêntico. Dá para voltar ao estado original a qualquer
momento.

## Hardware

| Item | Valor |
|---|---|
| Chip | ESP32-D0WDQ6-V3 (rev v3.0) |
| MAC | `b0:b2:1c:4e:33:3c` |
| Flash | 16 MB, DIO @ 80 MHz |
| PMU | AXP202 (o T-Watch V3 usa AXP2101 — não confundir) |
| Acelerômetro | BMA423 |
| Tela | 240x240 |
| eFuses | virgens: sem secure boot, sem criptografia de flash, nada com write-protect |

## Compilar e gravar

Requer [PlatformIO](https://platformio.org/) (extensão do VS Code ou `pip install platformio`).
Instalado nesta máquina: PlatformIO Core 6.1.19.

```bash
cd twatch
copy include\config.h.example include\config.h   # 1. crie sua cópia local (ignorada pelo git)
# 2. edite include/config.h com seu WIFI_SSID, WIFI_PASS e BITAXE_HOST reais
#    (BITAXE_HOST2 é opcional: preencha só se tiver um segundo Bitaxe)
pio run                    # compila (baixa core esp32 e libs na primeira vez)
pio run -t upload          # grava via COM5
pio device monitor         # log serial a 115200
```

Testes (Unity, rodam **no alvo** — exigem o relógio na USB):

```bash
pio test --upload-port COM5                    # todas as suítes
pio test --upload-port COM5 -f test_settings   # uma só
pio test -f test_settings --without-testing --without-uploading   # só compila
```

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

Conecte o celular e o portal abre sozinho. Dá para configurar rede WiFi, IP dos
Bitaxe, fuso horário, timeouts de WiFi e HTTP, bateria mínima, os tempos de
ociosidade antes do deep sleep, o brilho de cada tela e o intervalo da vigia
periódica. Salvar grava no NVS e reinicia o relógio.

O ponto de acesso vive no máximo **3 minutos** — é uma trava de bateria. Carregar
ou enviar a página rearma esse relógio, então digitar sem pressa não derruba o
portal.

O campo de senha do WiFi sempre chega vazio: a senha gravada nunca é devolvida
para a página, porque o ponto de acesso está ao alcance de qualquer vizinho.
Deixe vazio para manter a senha atual.

O IP do Bitaxe precisa ser IPv4 — `bitaxe.local` é recusado pelo formulário, e
com razão: o mDNS resolve para IPv6 e a API devolve 401.

## Dois Bitaxe

O campo "IP do segundo Bitaxe" no portal é opcional. Vazio, tudo funciona como
com um aparelho só.

Com dois, o toque no painel percorre um ciclo: relógio, dispositivo 1,
dispositivo 2, relógio. Os dois círculos no topo dizem onde você está.

Os dois são consultados na **mesma conexão WiFi**, quando o painel abre.
Alternar depois não liga o rádio — é redesenho do cache, instantâneo. Cada
dispositivo tem seu próprio slot na RTC memory, com idade e nota de falha
próprias: um aparelho desligado não contamina o outro.

## Vigia periódica

O relógio pode acordar sozinho, com a tela apagada, para conferir os mineradores
e vibrar se algum caiu. No portal, o campo "Vigia periodica" é o intervalo em
minutos; **0 desliga**, e é o padrão — é o único recurso que gasta bateria sem o
usuário pedir. A faixa aceita vai de 5 a 240 minutos.

O ciclo não acende a tela em momento algum: o `setup()` desvia antes de ligar o
backlight quando o despertar veio do temporizador, consulta os aparelhos, e volta
a dormir. O usuário percebe pela vibração — ou pelos dados já frescos quando
abrir o painel depois.

As vibrações são distintas de propósito: **uma** quando o painel abre e encontra
problema, **duas** quando a vigia encontra com a tela apagada. Sem rede, não
vibra: silêncio significa "está tudo bem", e não dá para afirmar isso sobre um
minerador que você não conseguiu perguntar.

A vigia não é armada quando a bateria está abaixo do mínimo configurado. É a
mesma trava que impede o painel de abrir com pouca carga.

## Brilho por tela

Relógio e painel têm brilho independente, ajustáveis de 10 a 100 por cento (o
piso existe porque abaixo disso a tela fica ilegível no sol — e o usuário não
conseguiria enxergar o portal para voltar atrás). O padrão deixa o relógio mais
escuro que o painel de propósito: é a tela que fica ligada por mais tempo. A
troca é um fade, não um corte.

## Contador de passos

O acelerômetro BMA423 conta passos e o número aparece na linha da data, à
direita. O contador não vem ligado pelo `watch->begin()`; é habilitado
explicitamente no boot.

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

## Detalhe que custa caro descobrir

A API do ESP-Miner valida private-network CORS. Uma requisição **sem o header
`Origin`** volta `401 Não Autorizado`, mesmo com IP correto e o dispositivo
saudável — parece equipamento inacessível quando não é. Por isso
`bitaxe_client.cpp` sempre envia:

```cpp
http.addHeader("Origin", String("http://") + BITAXE_HOST);
```

Use o IPv4 explícito, não `bitaxe.local`: o mDNS resolve para IPv6 e cai no mesmo 401.

Outra convenção da API: campos booleanos como `overheat_mode` trafegam como
**0/1 numérico**, não como booleano JSON. O cliente compara com `== 1`.

## Estado atual

O firmware está gravado no aparelho e as **nove suítes passam no alvo: 105 de 105
casos** (`pio test --upload-port COM5`) — as seis originais, mais `test_battery`
da Fase 1, `test_cycle` da Fase 2 e `test_watchdog` da Fase 3. `test_settings` e
`test_state` cresceram, mas não são suítes novas. O binário ocupa ~1,15 MB, 17,6%
do slot de app.

Nada em `src/` é alcançável por teste: o PlatformIO só compila `lib/` para dentro
das suítes. Por isso toda a lógica que dá para testar — bateria, ciclo de
navegação, máquina de estados, decisões da vigia, validação de configuração — mora
em `lib/bitaxe_core/`, e o que sobra em `src/` é o que fala com o hardware.

Confirmado no log de boot: o `settings_load` cai nas sementes do `config.h`
quando o NVS está vazio. O `nvs_open failed: NOT_FOUND` que aparece antes disso
**não é defeito** — é o `Preferences::begin` em modo leitura num namespace que
ainda não existe, o caminho normal de primeiro boot. Some depois do primeiro
salvamento pelo portal.

Falta a validação de ponta a ponta do portal com o celular: botão, formulário,
gravação e reinício.

Falta também confirmar no pulso se o contador de passos sobrevive ao deep sleep.
O `watch->begin()` reconfigura o BMA423 a cada boot, e o despertar reexecuta o
`setup()`, então o contador do chip pode zerar. Se zerar, é preciso acumular o
total numa variável `RTC_DATA_ATTR` — e incrementar o `SNAPSHOT_MAGIC` junto,
porque uma variável nova nessa área desloca o endereço do cache dos Bitaxe.
O acumulador **não** foi escrito por precaução: se o contador na verdade
persistir, ele passaria a somar os passos duas vezes a cada despertar.

Dois pontos antes pendentes, ambos resolvidos:

1. ~~**PSRAM.**~~ Confirmado no boot: `PSRAM enabled`, `psramFound()` verdadeiro,
   **4.192.123 bytes** disponíveis. O `-DBOARD_HAS_PSRAM` do `platformio.ini`
   está correto e fica. A dúvida existia porque o log do bootloader do firmware
   anterior estava silenciado (`CORE_DEBUG_LEVEL=0`) e a presença do driver no
   binário não prova nada — o core Arduino sempre o linka.
2. ~~**Versão do core.**~~ Confirmado: `espressif32@6.9.0` declara
   `framework-arduinoespressif32 ~3.20017.0` em seu `platform.json`, e o esquema
   `3.20017.x` corresponde ao Arduino core **2.0.17** — o mesmo do firmware
   anterior. Nada a ajustar aqui.

A API da `TTGO_T-Watch_Library` usada em `main.cpp` (`openBL()`,
`power->getBattPercentage()`, `getTouch()`) foi escrita sem a biblioteca em
mãos, e havia o risco de os nomes de método não baterem. Compila e grava sem
erro com a lib 1.4.2, então os nomes estão certos.

## Layout de partições

`partitions.csv` reproduz exatamente a tabela lida do aparelho, então este
firmware convive com o backup sem realinhar nada:

```
nvs       0x9000    20K
otadata   0xe000     8K
app0      0x10000  6400K
app1      0x650000 6400K
spiffs    0xc90000 3456K
coredump  0xff0000   64K
```

Os 6400K por slot de app são muito mais do que este projeto precisa. Se em algum
momento quiser recuperar espaço para SPIFFS, este é o arquivo a mexer — mas aí o
backup deixa de ser restaurável por cima sem apagar tudo antes.
