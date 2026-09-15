# BitAxe-Fix

Fork corrigido do [ESP-Miner](https://github.com/bitaxeorg/ESP-Miner) para mineradores **Bitaxe** (BM1366 / BM1368 / BM1370 Gamma): 11 correções sobre a v2.15.1 oficial, mais telemetria por MQTT.

**⬇️ [Baixar firmware](../../releases/latest)** · **🌐 [Site](https://bitaxefix.maqconnectsolucoes.com.br/?de=github)**

---

## A correção principal

O ESP-Miner v2.15.1 introduziu uma regressão na validação de nonce que, com **version-rolling** ativo, faz o firmware descartar shares válidas. O resultado é um Bitaxe que aparenta minerar normalmente — hashrate na tela, ASIC quente — enquanto as shares aceitas caem a zero na pool.

É silencioso: não há mensagem de erro, e só se percebe olhando o painel da pool.

## Todas as correções

### Crítico

| Correção | O que resolve |
|---|---|
| **Shares zeradas com version-rolling** | A regressão acima: shares válidas descartadas na validação de nonce |
| **Envelope de tensão e frequência por ASIC** | O original aceita qualquer valor via API — dá para aplicar tensão fora de faixa e danificar o chip |
| **Uso de memória liberada no Stratum** | Acesso a memória já liberada, causa de travamentos aleatórios |

### Estabilidade

| Correção | O que resolve |
|---|---|
| **Governador térmico gradual** | Redução suave de frequência no calor, em vez do corte abrupto |
| **Throttle por queda de tensão de entrada** | Protege quando a fonte não sustenta a carga |
| **Sensores mudos viram falha explícita** | Sensor sem resposta acusa erro em vez de reportar valor inválido |

### Segurança

| Correção | O que resolve |
|---|---|
| **Parsing de rede endurecido** | Validação de entrada nos dados vindos da pool |
| **Drivers de ASIC e sensores mais defensivos** | Checagem de limites nas respostas do hardware |

### Interface (AxeOS)

| Correção | O que resolve |
|---|---|
| **Vazamento de inscrição** | Vazamento no editor de dispositivos do modo swarm |
| **Senha do pool preservada ao salvar** | Salvar a página de pools sem redigitar a senha gravava um literal no lugar da senha real |
| **Varredura do swarm não derruba a interface** | Scan do swarm travava o AxeOS |

### Novidade

**Telemetria por MQTT** — cada Bitaxe publica hashrate, temperatura, potência e shares num tópico `bitaxe/<id>/telemetry`. Identificador derivado do MAC e editável na interface; intervalo configurável, TLS suportado.

---

## Instalação

Pela própria interface do Bitaxe, **sem cabo USB**:

1. Baixe `esp-miner.bin` e `www.bin` da [release](../../releases/latest) e confira o SHA256
2. No AxeOS, vá em **Settings → Firmware Update**
3. Envie primeiro o **`www.bin`** (interface), depois o **`esp-miner.bin`** (firmware)
4. Aguarde o reinício

> **Não renomeie os arquivos** — a interface só aceita esses nomes exatos. Alguns navegadores adicionam `(1)` ao baixar duas vezes.

A ordem importa: com a interface enviada primeiro, o novo firmware já encontra a versão correspondente do AxeOS ao subir.

### Conferir o hash

```sh
sha256sum esp-miner.bin www.bin          # Linux
shasum -a 256 esp-miner.bin www.bin      # macOS
certutil -hashfile esp-miner.bin SHA256  # Windows
```

Os hashes de cada versão estão nas [notas da release](../../releases/latest).

### Gravação por USB (recuperação)

Se a interface estiver inacessível:

```sh
esptool --port COM3 write-flash 0x10000 esp-miner.bin 0x410000 www.bin
```

| Arquivo | Offset | Partição |
|---|---|---|
| `esp-miner.bin` | `0x10000` | `factory` |
| `www.bin` | `0x410000` | `www` |

---

## Compilar

Requer **ESP-IDF v5.5.x** e Node.js 22+ (o frontend Angular é compilado junto e embutido como `www.bin`).

```sh
. ~/esp/v5.5.1/esp-idf/export.sh
git submodule update --init --recursive
idf.py set-target esp32s3
idf.py build
```

Saída: `build/esp-miner.bin` e `build/www.bin`.

### Estrutura

| Diretório | Conteúdo |
|---|---|
| `main/` | Aplicação: tarefas FreeRTOS, energia, térmica, servidor HTTP |
| `main/http_server/axe-os/` | Interface web (Angular 19) |
| `components/asic/` | Drivers por chip (BM1397/1366/1368/1370) |
| `components/stratum/` | Stratum V1; `stratum_v2/` para o V2 |
| `test/`, `test-ci/` | Testes Unity, executados em QEMU |

---

## Compatibilidade

**Testado em:** Bitaxe Gamma (BM1370, board 601)
**Deve funcionar em:** BM1366, BM1368 e BM1370 — mesma família de ASIC suportada pelo ESP-Miner upstream.

Requer ESP32-S3 com PSRAM octal (N16R8), o padrão dos Bitaxe.

## Voltar ao firmware oficial

Baixe `esp-miner.bin` e `www.bin` das [releases do ESP-Miner](https://github.com/bitaxeorg/ESP-Miner/releases) e envie pelo mesmo procedimento.

## Licença

**GPL-3.0**, herdada do ESP-Miner. Veja [LICENSE](LICENSE).

## Aviso

Software fornecido como está, sem garantia. Atualizar firmware envolve risco: uma queda de energia durante a gravação do `esp-miner.bin` deixa o aparelho com a versão anterior, mas uma queda durante a do `www.bin` pode deixar a interface inacessível — nesse caso, use a gravação por USB acima.

Não sou afiliado ao projeto Bitaxe nem ao bitaxeorg. O upstream oficial é https://github.com/bitaxeorg/ESP-Miner.
