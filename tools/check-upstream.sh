#!/usr/bin/env bash
# Compara este fork com o upstream bitaxeorg/ESP-Miner.
#
# Este fork foi importado como SNAPSHOT: nao compartilha nenhum commit com o
# upstream (git merge-base falha), entao "quantos commits atras" nao existe.
# O que da para medir e a proximidade de CONTEUDO contra cada release.
#
# Uso: tools/check-upstream.sh [--fetch]

set -uo pipefail
cd "$(git rev-parse --show-toplevel)"

UPSTREAM_URL="https://github.com/bitaxeorg/ESP-Miner.git"
SRC_PATHS=(main/ components/)

if ! git remote get-url upstream >/dev/null 2>&1; then
    echo "==> remote 'upstream' ausente; adicionando"
    git remote add upstream "$UPSTREAM_URL"
fi

if [ "${1:-}" = "--fetch" ] || [ -z "$(git tag -l 'v2.*' | head -1)" ]; then
    echo "==> buscando tags do upstream..."
    git fetch upstream --tags --quiet || { echo "ERRO: fetch falhou"; exit 1; }
fi

echo
echo "=== FORK ==="
printf 'versao local : %s\n' "$(cat version.txt 2>/dev/null || echo '?')"
printf 'HEAD         : %s\n' "$(git log -1 --format='%h %s' | cut -c1-70)"
printf 'branch       : %s\n' "$(git rev-parse --abbrev-ref HEAD)"

echo
echo "=== ULTIMA RELEASE DO UPSTREAM ==="
# Ordena por versao; separa estavel de pre-release (b/rc no nome).
latest_stable=$(git tag -l 'v2.*' | grep -vE 'b[0-9]+$|rc[0-9]+$' | sort -V | tail -1)
latest_any=$(git tag -l 'v2.*' | sort -V | tail -1)
printf 'estavel      : %s (%s)\n' "$latest_stable" \
    "$(git log -1 --format=%cs "$latest_stable" 2>/dev/null)"
if [ "$latest_any" != "$latest_stable" ]; then
    printf 'pre-release  : %s (%s)\n' "$latest_any" \
        "$(git log -1 --format=%cs "$latest_any" 2>/dev/null)"
fi

echo
echo "=== DISTANCIA DE CONTEUDO (menor = mais proximo) ==="
echo "Arquivos que diferem em ${SRC_PATHS[*]}, por release:"
for t in $(git tag -l 'v2.*' | grep -vE 'b[0-9]+$|rc[0-9]+$' | sort -V | tail -6); do
    stat=$(git diff --shortstat HEAD "$t" -- "${SRC_PATHS[@]}" 2>/dev/null)
    files=$(echo "$stat" | grep -oE '^ *[0-9]+' | tr -d ' ')
    printf '  %-10s %s arquivos\n' "$t" "${files:-0}"
done

echo
echo "=== MUDANCAS DO UPSTREAM AINDA NAO AVALIADAS ==="
base=$(cat .upstream-reviewed 2>/dev/null | tr -d '[:space:]')
if [ -n "$base" ] && git rev-parse -q --verify "$base^{commit}" >/dev/null 2>&1; then
    echo "Ultima release ja avaliada: $base"
    echo "Commits de $base ate $latest_stable:"
    git log --oneline --no-merges "$base..$latest_stable" -- "${SRC_PATHS[@]}" | head -40
    n=$(git rev-list --count --no-merges "$base..$latest_stable" -- "${SRC_PATHS[@]}")
    echo "  ... total: $n commits tocando codigo"
    echo
    echo "Ver o diff completo:"
    echo "  git diff $base..$latest_stable -- ${SRC_PATHS[*]}"
else
    echo "Nenhuma baseline registrada em .upstream-reviewed."
    echo "Crie uma com a release que este fork ja incorporou, ex:"
    echo "  echo $latest_stable > .upstream-reviewed"
fi
echo
