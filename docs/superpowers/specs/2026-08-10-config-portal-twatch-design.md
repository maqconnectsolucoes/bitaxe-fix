# T-Watch — botão de configuração e portal cativo

Data: 2026-08-10
Projeto: `twatch/` (TTGO T-Watch 2020 V1, ESP32-D0WDQ6-V3)
Antecessor: `2026-08-05-painel-bitaxe-twatch-design.md`

## Problema

Toda a configuração do relógio é compile-time. `include/config.h` carrega SSID,
senha, IP do Bitaxe, timeouts e limiar de bateria, e é lido diretamente por
`net.cpp` e `bitaxe_fetch.cpp`. Trocar de rede WiFi, mudar o IP do Bitaxe ou
ajustar o tempo de sono exige recompilar e regravar por USB.

Além disso, `net_sync_time()` chama `configTime(0, 0, NTP_SERVER)` — fuso UTC.
O relógio mostra três horas a mais que o horário de Brasília, e não há como
corrigir isso sem recompilar.

## Objetivo

Um botão na tela do relógio que abre um portal cativo: o relógio sobe um SoftAP,
o usuário conecta o celular, preenche um formulário web e salva. Os valores
passam a viver no NVS e sobrevivem a regravações de firmware.

## Escopo

**Nesta entrega:** botão, tela de config, portal, persistência em NVS dos campos
WiFi (SSID/senha), IP do Bitaxe, fuso horário, timeouts de WiFi e HTTP, bateria
mínima e os dois tempos de ociosidade do deep sleep.

**Fora desta entrega:** suporte a um segundo Bitaxe. Guardar um segundo IP é
trivial, mas obriga a redesenhar `panel_view` para dois aparelhos, duplicar o
cache na RTC memory (escassa) e dobrar o tempo de rádio ligado por consulta.
Vira spec própria depois desta.

## Decisões e o porquê

**Portal cativo, não teclado na tela.** A tela tem 240x240. Um QWERTY nela dá
teclas de ~24x30 px num touch capacitivo, e digitar senha de WiFi assim é
penoso, além de ser bem mais código de UI do que o portal inteiro. O celular já
tem teclado.

**Chave por campo no NVS, não um blob.** Cada campo é uma chave própria no
namespace `twatch`, lida com o valor do `config.h` como *default* do
`getString`/`getInt`/`getUShort`. Com isso, "NVS vazio usa `config.h`" sai de
graça e por campo, e acrescentar um campo no futuro não invalida os já gravados.
Um blob versionado exigiria descartar tudo a cada mudança de layout.

**`config.h` vira semente de primeiro boot.** Continua existindo, continua no
`.gitignore`, continua obrigatório para compilar — mas rebaixado a default de
fábrica. Só o `settings_store` o lê, e só para semear. O relógio continua
funcionando na primeira gravada sem passar pelo portal.

**Engrenagem no rodapé, não toque longo nem botão PEK.** Toque longo é gesto
invisível: sem descoberta e fácil de esquecer. O PEK já tem o papel de acordar o
aparelho, e dar dois papéis a ele confunde. A faixa do rodapé é alvo grande e
preserva o gesto atual — tocar no meio da tela continua abrindo o painel.

**Salvar reinicia o relógio.** `setup()` relê tudo do zero e não sobra estado
velho de rádio, de NTP nem de sessão HTTP. Reconfigurar o stack WiFi ao vivo,
saindo de AP para STA, seria mais código e mais modos de falha para economizar
dois segundos.

## Arquitetura

### Arquivos novos

| Arquivo | Papel | Testável |
|---|---|---|
| `lib/bitaxe_core/settings.h/.cpp` | POD `Settings`, `settings_validate()`, `settings_apply_form()` | sim |
| `lib/bitaxe_core/ui_layout.h/.cpp` | `clock_hit(x, y)` — a função pura por trás do botão | sim |
| `src/settings_store.h/.cpp` | `settings_load()` / `settings_save()` sobre `Preferences` (NVS) | não |
| `src/config_portal.h/.cpp` | SoftAP + `DNSServer` + `WebServer`, formulário e POST | não |
| `src/config_view.h/.cpp` | desenha a tela de config | não |

A lógica testável vive em `lib/` porque o PlatformIO não compila `src/` para os
testes — só `lib/`. É o mesmo motivo pelo qual `bitaxe_parse`, `metrics` e
`state_machine` já estão lá.

### Estrutura `Settings`

```c
struct Settings
{
    char wifiSsid[33];        // 1..32 + NUL
    char wifiPass[64];        // vazia (rede aberta) ou 8..63 + NUL
    char bitaxeHost[16];      // IPv4 estrito, "255.255.255.255" + NUL
    int16_t tzMinutes;        // -720..+840, passos de 30
    uint32_t wifiTimeoutMs;   // 1000..30000
    uint32_t httpTimeoutMs;   // 1000..15000
    uint8_t batteryMinPct;    // 0..50
    uint32_t idleRelogioMs;   // 2000..60000
    uint32_t idlePainelMs;    // 5000..120000
};
```

Todos os campos de tempo são `uint32_t`, não `uint16_t`: `idlePainelMs` chega a
120000, que estoura os 65535 de um `uint16_t`. Uniformizar evita a próxima faixa
que cresça sem ninguém reparar.

`settings_defaults()` **não** fica aqui. A semente vem das macros do `config.h`,
que é gitignored; se `lib/bitaxe_core/settings.cpp` dependesse dele, os testes
deixariam de compilar num clone limpo. A semente é montada em
`src/settings_store.cpp`, que é o único módulo que ainda lê `config.h`.

### Mudanças em código existente

- `bitaxe_fetch()` passa a receber host e timeout por parâmetro:
  `bitaxe_fetch(const char *host, uint32_t timeoutMs, BitaxeStatus &out, int &httpCode)`.
  Deixa de incluir `config.h`. O header `Origin` continua sendo montado a partir
  do host recebido.
- `net_connect()` recebe SSID, senha e timeout por parâmetro.
- `net_sync_time()` recebe `tzMinutes` e chama `configTime(tzMinutes * 60, 0, NTP_SERVER)`.
- `state_machine`: novo `STATE_CONFIG`, novo `EVENT_TOUCH_CONFIG`, novo
  `IDLE_TIMEOUT_CONFIG_MS = 180000`. A assinatura **muda**, porque dois dos
  tempos de ociosidade deixaram de ser constantes de compilação:

  ```c
  struct IdleTimeouts { uint32_t relogioMs; uint32_t painelMs; };
  AppState state_next(AppState current, AppEvent event, uint32_t idleMs,
                      const IdleTimeouts &t);
  ```

  `IDLE_TIMEOUT_CONFIG_MS` continua constante dentro da função. Os testes
  existentes em `test/test_state` passam a construir um `IdleTimeouts` com os
  valores de hoje (5000 / 15000) e seguem verificando o mesmo comportamento.
- `clock_view_draw()` desenha a faixa da engrenagem no rodapé. O teste de acerto
  fica em `ui_layout`, não aqui.
- `snapshot_store` ganha `snapshot_clear()`.
- `main.cpp` carrega `Settings` no `setup()` e repassa aos módulos; `loop()`
  ganha o ramo de `STATE_CONFIG`.

## Máquina de estados

```
DEEP_SLEEP --WAKE--------------> RELOGIO
RELOGIO    --TOUCH-------------> PAINEL
RELOGIO    --TOUCH_CONFIG------> CONFIG
RELOGIO    --TICK >= 5s--------> DEEP_SLEEP
PAINEL     --TOUCH-------------> RELOGIO
PAINEL     --TOUCH_CONFIG------> RELOGIO      (painel não tem engrenagem; a
                                               função fica total, sem buraco)
PAINEL     --TICK >= 15s-------> DEEP_SLEEP
CONFIG     --TOUCH-------------> RELOGIO      (sai sem salvar, derruba o AP)
CONFIG     --TOUCH_CONFIG------> RELOGIO
CONFIG     --TICK >= 180s------> DEEP_SLEEP   (trava de bateria)
```

Os tempos de ociosidade de relógio e painel passam a vir de `Settings`; o de
config é constante de compilação, porque é uma trava de segurança de bateria e
não faz sentido o usuário poder afrouxá-la pelo próprio portal.

### O contador de ociosidade em CONFIG

Requisição HTTP atendida pelo portal conta como interação e rearma o contador.
Sem isso o relógio dorme no meio da digitação no celular, já que o usuário não
está tocando no relógio. `config_portal_poll()` devolve `true` quando atendeu
alguma requisição desde a última chamada, e `main.cpp` usa isso para atualizar
`lastInteraction`.

## Fluxo

### Entrar em config

1. Toque na faixa do rodapé (y >= 192) na tela do relógio.
2. `config_portal_begin()` sobe o SoftAP `T-Watch-Setup` com WPA2 e o DNS
   respondendo `*` -> `192.168.4.1`.
3. `config_view_draw()` pinta nome do AP, senha, URL, tempo restante e, se a
   bateria estiver abaixo do limiar, o aviso `bateria baixa`. **Aviso, não
   bloqueio**: config é justamente o que se precisa quando algo quebrou. Isso
   difere do painel, que aborta a conexão com bateria baixa (`main.cpp:95`).
4. `loop()` roda `dns.processNextRequest()` + `server.handleClient()` a cada
   ~5 ms e redesenha o contador uma vez por segundo.

### Salvar

1. `GET /` devolve o formulário preenchido com os valores atuais. **A senha do
   WiFi nunca é devolvida na página**: o campo vem vazio com placeholder
   `(inalterada)`. Enviar vazio mantém a gravada. Um AP aberto ao alcance dos
   vizinhos não é lugar para trafegar a senha da casa em texto claro.
2. `POST /save` monta um `Settings` candidato com `settings_apply_form()` e roda
   `settings_validate()`. Reprovou: devolve a página com a mensagem de erro e os
   valores digitados, sem gravar nada.
3. Passou: `settings_save()` grava as chaves, `snapshot_clear()` apaga o cache da
   RTC memory e `g_timeIsValid` volta a `false` — se o fuso ou o IP mudaram, hora
   e cache antigos passaram a estar errados, e a RTC memory sobrevive a reset por
   software, então precisa ser limpa de propósito.
4. A página de confirmação é enviada, a tela mostra `salvo, reiniciando`, e
   `ESP.restart()` é chamado após ~1 s.

### Sair sem salvar

Toque em qualquer ponto da tela de config: `config_portal_end()` derruba AP e
DNS, e o estado volta para `RELOGIO`.

## Validação

| Campo | Regra | Motivo |
|---|---|---|
| SSID | 1–32 caracteres | limite do 802.11 |
| Senha | vazia ou 8–63 caracteres | WPA2 não aceita 1–7 |
| IP do Bitaxe | IPv4 estrito, 4 octetos 0–255 | `bitaxe.local` resolve por IPv6 e devolve 401 (README:63); aceitar hostname plantaria esse bug |
| Fuso | `<select>` de −12:00 a +14:00 em passos de 30 min | impossível digitar errado |
| Timeout WiFi | 1000–30000 ms | |
| Timeout HTTP | 1000–15000 ms | |
| Bateria mínima | 0–50 % | acima disso o painel nunca abriria |
| Ociosidade relógio | 2000–60000 ms | |
| Ociosidade painel | 5000–120000 ms | |

## Erros

- **SoftAP não sobe** (`WiFi.softAP()` devolve `false`): mensagem na tela e
  retorno para `RELOGIO`.
- **`Preferences` grava 0 bytes**: a página responde `falha ao gravar` e o
  relógio **não** reinicia — o estado anterior segue válido.
- **Queda de energia no meio da gravação**: pode deixar chaves misturadas. Como
  a validação roda sobre o conjunto inteiro antes de qualquer escrita, toda
  chave gravada é individualmente válida; o pior caso é reabrir o portal e
  salvar de novo. Não haverá journaling para isso.
- **Validação reprovada**: nada é escrito, a página volta com o erro.

## Testes

Rodam no alvo via Unity (`test_port = COM5`, `pio test --upload-port COM5`).

**`test/test_settings`** (novo)
- cada regra da tabela de validação, no limite e fora dele
- `settings_apply_form()` com senha vazia preserva a gravada
- `settings_apply_form()` com senha preenchida substitui
- um `Settings` inteiramente preenchido com valores plausíveis passa em
  `settings_validate()` (guarda contra uma regra escrita ao contrário)

**`test/test_layout`** (novo)

A faixa de config ocupa os 48 px de baixo da tela de 240: `clock_hit()` devolve
`HIT_CONFIG` para `y >= 192` e `HIT_PAINEL` abaixo disso.
- `clock_hit(120, 220)` -> `HIT_CONFIG`
- `clock_hit(120, 100)` -> `HIT_PAINEL`
- `clock_hit(120, 192)` -> `HIT_CONFIG` (primeira linha da faixa)
- `clock_hit(120, 191)` -> `HIT_PAINEL` (última linha fora dela)

**`test/test_state`** (estendido)
- `RELOGIO + TOUCH_CONFIG` -> `CONFIG`
- `CONFIG + TOUCH` -> `RELOGIO`
- `CONFIG + TICK 179000` -> `CONFIG`
- `CONFIG + TICK 180000` -> `DEEP_SLEEP`
- `PAINEL + TOUCH_CONFIG` -> `RELOGIO`

**Verificação manual** (sem automação possível), a executar no aparelho:
1. Tocar no rodapé abre a tela de config e o AP aparece na lista do celular.
2. Conectar ao AP faz o celular abrir o portal sozinho.
3. O formulário chega preenchido, com o campo de senha vazio.
4. Salvar com IP inválido devolve erro e não regrava.
5. Salvar válido reinicia o relógio, que conecta na rede configurada.
6. Com fuso −3, a tela do relógio mostra o horário de Brasília.
7. Sem tocar em nada, a tela de config dorme após 180 s.
8. Digitando no celular por mais de 180 s, o relógio **não** dorme.

## Impacto no tamanho do binário

`WebServer` e `DNSServer` acrescentam algo na casa de 50–80 KB. O slot de app
tem 6400 KB (`partitions.csv`), então não há aperto.
