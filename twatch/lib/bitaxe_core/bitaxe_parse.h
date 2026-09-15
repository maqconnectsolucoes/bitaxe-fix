#pragma once

#include "bitaxe_status.h"

// Devolve false e deixa `out` intocada se o JSON for inválido OU se faltar
// algum campo essencial (hashRate_10m, power) — meio dado é pior que dado
// velho, então a resposta incompleta é descartada inteira e o cache do
// chamador é preservado.
// Campos opcionais ausentes (frequency, coreVoltageActual, sharesAccepted,
// bestDiff, hostname) viram zero (ou string vazia), nunca lixo.
bool bitaxe_parse(const char *json, BitaxeStatus &out);
