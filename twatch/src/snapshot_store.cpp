#include "snapshot_store.h"

#include <esp_attr.h>  // RTC_DATA_ATTR

// RTC slow memory: sobrevive ao deep sleep, e apagada por corte total de energia.
// Vale lembrar que so POD pode morar aqui — nada de String ou ponteiros.
RTC_DATA_ATTR static BitaxeStatus g_snapshot[SNAPSHOT_SLOTS];
RTC_DATA_ATTR static uint32_t g_snapshotEpoch[SNAPSHOT_SLOTS];

// Magic por slot, nao global: um aparelho offline nao pode invalidar o cache
// do outro.
RTC_DATA_ATTR static uint32_t g_magic[SNAPSHOT_SLOTS];

#define SNAPSHOT_MAGIC 0x8175A1E2

bool snapshot_load(uint8_t slot, BitaxeStatus &out)
{
    if (slot >= SNAPSHOT_SLOTS || g_magic[slot] != SNAPSHOT_MAGIC) {
        return false;
    }
    out = g_snapshot[slot];
    return true;
}

void snapshot_save(uint8_t slot, const BitaxeStatus &s, uint32_t epoch)
{
    if (slot >= SNAPSHOT_SLOTS) {
        return;
    }
    g_snapshot[slot] = s;
    g_snapshotEpoch[slot] = epoch;
    g_magic[slot] = SNAPSHOT_MAGIC;
}

uint32_t snapshot_age(uint8_t slot, uint32_t nowEpoch)
{
    if (slot >= SNAPSHOT_SLOTS || g_magic[slot] != SNAPSHOT_MAGIC ||
        nowEpoch <= g_snapshotEpoch[slot]) {
        return 0;
    }
    return nowEpoch - g_snapshotEpoch[slot];
}

void snapshot_clear(void)
{
    // Zerar o magic basta: e ele que snapshot_load e snapshot_age conferem.
    for (uint8_t i = 0; i < SNAPSHOT_SLOTS; i++) {
        g_magic[i] = 0;
        g_snapshotEpoch[i] = 0;
    }
}
