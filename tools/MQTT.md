# Telemetria por MQTT — estado da implementação

Publicação da telemetria de cada Bitaxe num broker MQTT externo, com TLS 1.2 e CA própria,
para levar os dados para fora da rede local sem abrir porta no roteador.

Divergência deliberada do upstream: o `bitaxeorg/ESP-Miner` não tem MQTT em lugar nenhum.

## Decisões

| Questão | Decisão |
|---|---|
| Origem | Cada Bitaxe publica a si mesma, sem agregador de swarm |
| Tópico | `bitaxe/<identificacao>/telemetry`, um JSON completo por mensagem |
| Intervalo | Configurável, padrão 30 s |
| Autenticação | Usuário e senha, além do TLS |
| Certificado | CA em PEM colado ou enviado pela tela, como a página de pools já faz |
| Falha de conexão | Reconexão silenciosa; a mineração nunca depende do MQTT |

Fora de escopo por ora: receber comandos por MQTT (superfície de ataque nova, decisão à parte),
descoberta automática do Home Assistant e agregação de swarm.

## Viabilidade verificada

Levantado antes de escrever código, com ESP-IDF v5.5.3:

- **Componente**: `esp-mqtt` é nativo do framework, não precisa ser baixado.
- **TLS 1.2**: já habilitado (`CONFIG_MBEDTLS_SSL_PROTO_TLS1_2=y` no `sdkconfig`), e o mbedtls já
  está linkado por causa do TLS das pools. O custo em flash é só a camada de protocolo.
- **Flash**: o binário usa 1,68 MB de 4 MB, folga de 2,4 MB.
- **Configuração**: cada entrada aceita até 3999 bytes (`NVS_STR_LIMIT`, `main/nvs_config.c:23`) e
  um CA em PEM tem cerca de 2 KB. Cabe.

## Arquitetura

Tarefa nova, `main/tasks/mqtt_task.c`, criada em `main/main.c` junto da `statistics_task`
(~linha 191), portanto depois do portão `while (!is_connected)` de `main.c:166`, que já garante
rede no boot. Como a rede pode cair depois, o laço revalida `wifi_is_connected()`
(`components/connect/connect.c:872`) a cada volta, como o stratum faz (`stratum_v1_task.c:202`).

Criação com `xTaskCreateWithCaps(..., MALLOC_CAP_SPIRAM)`, stack 8192, prioridade 3 — os mesmos
parâmetros da `statistics_task`, que tem perfil equivalente: periódica e não crítica para a
mineração.

**A mineração nunca depende do MQTT.** Falha de broker só afeta a publicação.

### Payload

Reaproveita `system_api_add_telemetry()` (`main/http_server/system_api_json.c:41`), que já monta
hashrate, temperaturas, potência, shares, uptime e falhas. A tarefa acrescenta o hostname e publica
com `esp_mqtt_client_publish(client, topic, json, 0, 1, 0)` — QoS 1, sem retain.

Tópico `bitaxe/<identificacao>/telemetry`. A identificação vem de
`NVS_CONFIG_MQTT_DEVICE_ID` e, quando nada foi gravado, cai no padrão derivado do MAC
(`bitaxe-3cdc755aa5fc`). **Não usa o hostname**: ele é editável pelo usuário e duas placas saem de
fábrica com o mesmo, o que colidiria no broker. O MAC é único por construção.

Reaproveitar em vez de duplicar garante que o JSON do MQTT e o da API não divirjam com o tempo.

### Chaves de configuração

Em `main/nvs_config.h` (enum `NvsConfigKey`) e na tabela `settings[]` de `main/nvs_config.c:53`,
seguindo o padrão de `NVS_CONFIG_HOSTNAME`:

| Chave | Tipo | `rest_name` | Limites |
|---|---|---|---|
| `MQTT_ENABLED` | BOOL | `mqttEnabled` | 0-1 |
| `MQTT_HOST` | STR | `mqttHost` | 0-128 |
| `MQTT_PORT` | U16 | `mqttPort` | 1-65535, padrão 8883 |
| `MQTT_USER` | STR | `mqttUser` | 0-64 |
| `MQTT_PASS` | STR | `mqttPass` | 0-128 |
| `MQTT_CA_CERT` | STR | `mqttCaCert` | 0-3000 |
| `MQTT_INTERVAL` | U16 | `mqttInterval` | 5-3600, padrão 30 |

A validação de tipo e tamanho vem de graça de `check_settings_and_update()`
(`main/http_server/http_server.c:814`), que já confere `min`/`max` de todo `TYPE_STR`.

## Arquivos que a funcionalidade toca

**Firmware**
- `main/tasks/mqtt_task.c` e `.h` — a criar: laço de publicação, callback de eventos, reconexão.
- `main/nvs_config.h` / `.c` — as sete chaves (feito).
- `main/main.c` — criação da tarefa (~linha 191).
- `main/CMakeLists.txt` — `./tasks/mqtt_task.c` em `SRCS` e `"mqtt"` em `PRIV_REQUIRES`.
- `main/http_server/http_server.c` — senha com placeholder (feito).
- `main/http_server/system_api_json.c` — exposição dos campos (feito).

**AxeOS**
- `src/app/components/mqtt/` — `.ts`, `.html` e `.spec.ts` (feito).
- `src/app/app.module.ts`, `app-routing.module.ts`, `layout/app.menu.component.ts` — registro,
  rota e menu (feito).
- `main/http_server/openapi.yaml` (feito) mais `npm run generate:api`, e os mocks de
  `src/app/services/system.service.ts` (feito).

## Fase 1 — configuração (concluída)

Commits `c7eaa616` e `23af25be`, mais a tela e o build desta sessão.

Pronto:

- Sete chaves em `main/nvs_config.h`/`.c`: `mqttEnabled`, `mqttHost`, `mqttPort` (padrão 8883),
  `mqttUser`, `mqttPass`, `mqttCaCert` (até 3000 bytes) e `mqttInterval` (5 a 3600 s, padrão 30).
- `main/http_server/http_server.c`: o PATCH ignora o literal `*****` em `mqttPass`. Sem isso,
  salvar a tela sem redigitar a senha apagaria a do broker — o mesmo defeito que as senhas de pool
  tinham. O literal virou `PASSWORD_PLACEHOLDER`, reaproveitado por `update_pool_nvs`.
- `main/http_server/system_api_json.c`: expõe os campos; a senha sai sempre como `*****` quando há
  algo gravado, nunca o valor.
- `main/http_server/openapi.yaml`: os campos nos schemas de leitura e de escrita.
- `src/app/validators/hostname.validator.ts` com 10 casos. O `addressValidator` do swarm não servia:
  recusa domínio completo como `mqtt.exemplo.com.br`.

### Concluído nesta sessão

1. **`idf.py build`** passou (exit 0) com o código C da fase 1, que até então nunca tinha sido
   compilado. Binário em 1.733.120 bytes, 59% da partição livre.
2. **Tela MQTT**, em `src/app/components/mqtt/`, modelada em `pool.component`: envio só dos campos
   `dirty`, senha com placeholder, `textarea` do certificado com upload, `takeUntil` no `OnDestroy`
   e `loadOrFail` no carregamento. Registrada em `app.module.ts`, `app-routing.module.ts` (rota
   `/mqtt`) e no menu, entre Network e Theme.
3. **`mqtt.component.spec.ts`** com 8 casos, escritos antes do componente: carga do formulário,
   PATCH só com campos alterados, senha nunca enviada como placeholder (nem quando o campo é
   tocado), `mqttEnabled` numérico, host inválido bloqueando o Save, obrigatoriedade do host só
   enquanto habilitado, e ausência de reação após o destroy.
4. **Mocks** de `system.service.ts` com os sete campos.
5. **Campo de identificação** na tela, pré-preenchido e editável, com o tópico resultante
   mostrado abaixo dele. Recusa `/`, `+`, `#` e espaço, que quebrariam o tópico.

Suíte completa do AxeOS: 105 testes, 0 falhas. Build de produção (AOT) limpo.

Decisão de desenho tomada aqui: o host só é obrigatório enquanto `mqttEnabled` está ligado — senão
desligar o MQTT ficaria bloqueado por um campo que deixou de importar.

## Fase 2 — cliente MQTT (implementada, ainda não validada contra um broker real)

`main/tasks/mqtt_task.c`, criada em `main/main.c` junto da `statistics_task` (~linha 191), portanto
depois do portão `while (!is_connected)`. Revalidar `wifi_is_connected()` a cada volta, como o
stratum faz, porque a rede pode cair depois. Criar com `xTaskCreateWithCaps` em PSRAM, stack 8192,
prioridade 3.

O payload reaproveita `system_api_add_telemetry()` (`system_api_json.c:41`) em vez de duplicar a
lógica, garantindo que o JSON do MQTT e o da API não divirjam.

Dois pontos de atenção verificados no ESP-IDF v5.5.3:

- `"mqtt"` precisa entrar em `PRIV_REQUIRES` de `main/CMakeLists.txt`. Não vai no
  `idf_component.yml`, que é só para componentes gerenciados.
- O certificado **não é copiado** pelo cliente (`mqtt_client.h:263`): o ponteiro passado em
  `broker.verification.certificate` tem que continuar válido enquanto o cliente viver.

### O que foi feito

`main/tasks/mqtt_task.c` e `.h`, criada em `main/main.c` junto da `autotune_task`, com
`xTaskCreateWithCaps` em PSRAM, stack 8192, prioridade 3. `"mqtt"` entrou em `PRIV_REQUIRES`.
Compila limpo; binário em 0x1ae3e0 bytes, 58% da partição livre.

- **Oitava chave, `mqttDeviceId`** (`.max = 64`), exposta na API. `system_api_json.c` **sempre**
  devolve um valor — o gravado ou o padrão do MAC — para a tela chegar preenchida.
- **O payload é só telemetria.** Em vez de `system_api_get_full_json()`, foi criado
  `system_api_get_telemetry_json()`: o JSON completo carrega SSID, usuário da pool e hostname, que
  não têm por que viajar para um broker externo.
- **TLS é decidido pelo certificado**: com CA preenchida o transporte é SSL, sem ela é TCP puro.
  O ponteiro da CA é um `static` do módulo, pela armadilha registrada acima.
- **A identificação é relida a cada (re)conexão**, então editar na tela não exige reboot.
- Falha de broker nunca é fatal: `MQTT_EVENT_ERROR` só loga, e o laço revalida
  `wifi_is_connected()` a cada volta.

### Pendente

1. **Validar contra um broker real** — nada foi publicado ainda. Falta host, usuário, senha e CA.
2. **Fase 3**: estado da conexão e horário da última publicação, na API e na tela.

## Fase 3 — estado na interface (não iniciada)

Estado da conexão (`conectado`, `desconectado`, `erro de TLS`) e horário da última publicação
bem-sucedida, expostos na API e mostrados na tela.

## Verificação no aparelho

1. Preencher host, porta 8883, usuário, senha e CA; salvar e recarregar para conferir que
   persistiu e que a senha volta como `*****`.
2. Salvar de novo **sem** tocar na senha e confirmar que a conexão continua. É o defeito que as
   pools tinham.
3. Na VPS: `mosquitto_sub -h <host> -p 8883 --cafile ca.crt -u <user> -P <senha> -t 'bitaxe/#' -v`,
   conferindo um JSON a cada 30 s com os mesmos números de `/api/system/info`.
4. Derrubar o broker alguns minutos: o hashrate não pode oscilar e a publicação volta sozinha.
5. Uma hora rodando com `freeHeap` estável.
6. Com `mqttEnabled` desligado, nenhuma conexão de saída.
