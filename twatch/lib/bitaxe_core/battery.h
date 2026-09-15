#pragma once

#include <stdint.h>

// Percentual desconhecido. Existe porque o AXP202 tem dois modos de falhar e
// nenhum deles devolve um numero utilizavel — ver battery_percent.
#define BATTERY_UNKNOWN (-1)

// Segmentos desenhados dentro da pilha.
#define BATTERY_SEGMENTS 6

// Faixa da estimativa por tensao. Celula de litio de uma serie.
#define BATTERY_MV_EMPTY 3300
#define BATTERY_MV_FULL 4200

enum BatteryLevel
{
    BATTERY_LEVEL_UNKNOWN,
    BATTERY_LEVEL_LOW,   // abaixo do minimo configurado
    BATTERY_LEVEL_MID,   // do minimo ate 50%
    BATTERY_LEVEL_HIGH,  // acima de 50%
};

// `axpPercent` e o retorno cru de getBattPercentage(); `millivolts` o de
// getBattVoltage(), ou 0 quando indisponivel.
int battery_percent(int axpPercent, uint16_t millivolts);

BatteryLevel battery_level(int percent, uint8_t minPct);

// 0..BATTERY_SEGMENTS. Arredonda para cima: 1% ja acende um segmento.
int battery_segments(int percent);
