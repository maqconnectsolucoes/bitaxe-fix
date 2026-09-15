#include "battery.h"

int battery_percent(int axpPercent, uint16_t millivolts)
{
    // O registrador AXP202_BATT_PERCENTAGE devolve 0 para "sem bateria" e para
    // bit de validade baixo, e negativo quando o chip nao inicializou. So a
    // faixa 1..100 e informacao.
    if (axpPercent > 0 && axpPercent <= 100) {
        return axpPercent;
    }

    if (millivolts == 0) {
        return BATTERY_UNKNOWN;
    }
    if (millivolts <= BATTERY_MV_EMPTY) {
        return 0;
    }
    if (millivolts >= BATTERY_MV_FULL) {
        return 100;
    }

    // Reta entre os dois extremos. Grosseira de proposito: o numero decide
    // "da para ligar o radio?", nao prediz autonomia.
    const int32_t span = BATTERY_MV_FULL - BATTERY_MV_EMPTY;
    return (int) (((int32_t) (millivolts - BATTERY_MV_EMPTY) * 100) / span);
}

BatteryLevel battery_level(int percent, uint8_t minPct)
{
    if (percent < 0 || percent > 100) {
        return BATTERY_LEVEL_UNKNOWN;
    }
    if (percent < (int) minPct) {
        return BATTERY_LEVEL_LOW;
    }
    if (percent <= 50) {
        return BATTERY_LEVEL_MID;
    }
    return BATTERY_LEVEL_HIGH;
}

int battery_segments(int percent)
{
    if (percent <= 0) {
        return 0;
    }
    if (percent >= 100) {
        return BATTERY_SEGMENTS;
    }
    // Teto da divisao: 1% precisa acender um segmento, senao a pilha fica vazia
    // com o aparelho ainda ligado.
    return (percent * BATTERY_SEGMENTS + 99) / 100;
}
