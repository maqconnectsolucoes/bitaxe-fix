# Acompanhando o upstream (bitaxeorg/ESP-Miner)

## Situação deste fork

Este repositório **não compartilha histórico git com o upstream**. O primeiro
commit é `fba3b43 "Initial commit of ESP-Miner firmware and AxeOS frontend"`,
ou seja, o código entrou como *snapshot* (cópia de arquivos), não como fork
via GitHub. Consequência prática:

- `git merge-base HEAD upstream/master` **falha** (nenhum ancestral comum).
- "Estamos N commits atrás do upstream" **não é uma pergunta respondível** aqui.
  Os `git rev-list --count` entre os dois lados contam históricos paralelos
  inteiros (1346 vs 104), não distância real.
- `git merge upstream/master` tentaria unir duas árvores sem ancestral comum —
  não faça isso sem entender que resultaria em conflito em quase todo arquivo.

O que **é** mensurável é a proximidade de *conteúdo* com cada release. A curva
de "arquivos que diferem" cai de 350 (v2.13.2) para 169 (v2.15.0) e estabiliza,
indicando que o snapshot foi tirado por volta da **v2.15.0**. Os 169 arquivos
restantes são, em boa parte, as modificações próprias deste fork (autotune,
governadores de throttle, hardening, AxeOS).

## Uso

```bash
tools/check-upstream.sh          # usa as tags já buscadas
tools/check-upstream.sh --fetch  # busca tags novas do upstream antes
```

O script adiciona o remote `upstream` sozinho se faltar, e busca as tags na
primeira execução.

## `.upstream-reviewed`

Arquivo de uma linha com a última release do upstream **já avaliada** para este
fork. É a baseline que faz o script listar apenas o que é novo desde então.

Ao terminar de revisar/portar as mudanças de uma release, avance a baseline:

```bash
echo v2.15.1 > .upstream-reviewed
git commit -am "chore: baseline do upstream em v2.15.1"
```

## Como portar uma correção do upstream

Como não há ancestral comum, `git cherry-pick` de um commit do upstream
geralmente falha. O caminho confiável é aplicar a mudança por conteúdo:

```bash
# ver o que o commit fez
git show 78a03e3b -- main/ components/

# aplicar só o arquivo que interessa, revisando o resultado
git diff v2.15.0..v2.15.1 -- main/http_server/http_server.c
```

Depois adapte à mão, já que os arquivos daqui divergiram do upstream.

## Estado em relação à v2.15.1

Os quatro commits de código da v2.15.1 já foram portados (commit `daad7d53`):
reconexão com `keep_alive_enable` (#1913), compatibilidade WPA2/3 (#1912),
IPs do swarm (#1905) e a mensagem de download da página de update (#1907).
O baseline registrado em `.upstream-reviewed` é `v2.15.1`.

A pré-release `v2.15.2rc0` adiciona drivers dos ASICs **BM1372/BM1373** e
melhorias de display para Bitaxe Color — relevante apenas se for dar suporte a
esses chips.

## Divergências deliberadas do upstream

Estas mudanças são específicas deste fork; ao portar código do upstream que
toque nestes pontos, não sobrescreva o comportamento local.

| Área | Divergência |
|---|---|
| Envelope de tensão/frequência (`main/tuning_limits.*`, `main/device_config.h`, `components/asic/asic_common.c`) | O upstream não valida `coreVoltage`/`frequency` no servidor: `{"coreVoltage":1999}` é aceito e aplicado. Aqui há um envelope por ASIC — presets sem `overclockEnabled`, teto absoluto por chip com ele — aplicado no PATCH HTTP, no BAP e defensivamente no `power_management_task` (inclusive sobre NVS já gravado). |
| Senha do pool (`main/http_server/http_server.c`, `pool.component.ts`) | No upstream, salvar a página de pools sem redigitar a senha grava `"x"`: o Angular remove o placeholder `*****` e o C usa o default para campo ausente. Aqui, campo ausente ou `*****` preserva a senha armazenada, e o Angular envia o placeholder. |
| `build-info.json.gz` (`generate-version.js`) | O upstream gzipa o placeholder `{"version":"dev"}` antes de gerar a versão, e o firmware prefere o `.gz`: o rodapé mostra "dev". Aqui o script sobrescreve o `.gz`. |
| `showNewBlock` (`swarm.component.ts`) | O firmware emite JSON boolean; o upstream compara `=== 1` no swarm e nunca mostra "Block found". Aqui a comparação é truthy. |
| Versão (`version.txt`) | `V0.x` no lugar do `git describe`, em firmware e AxeOS (ver skill `app-landing-page`). |
