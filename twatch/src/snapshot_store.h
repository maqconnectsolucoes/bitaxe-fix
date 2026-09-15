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
