#ifndef TUNING_LIMITS_H_
#define TUNING_LIMITS_H_

#include <stdbool.h>
#include <stdint.h>
#include "asic_common.h"
#include "global_state.h"

// Server-side envelope for the two settings that can physically damage the
// ASIC: core voltage (mV) and frequency (MHz). The envelope comes from the
// chip's own preset tables plus its absolute overclock ceiling in
// device_config.h, so every writer (REST, BAP, boot-time NVS read) agrees on
// the same limits instead of each carrying its own constants.
//
// `overclock` is passed in rather than read here because a REST PATCH may
// carry overclockEnabled in the same body as the value being checked.

asic_range_t tuning_voltage_range(const GlobalState * gs, bool overclock);
asic_range_t tuning_frequency_range(const GlobalState * gs, bool overclock);

bool tuning_voltage_allowed(const GlobalState * gs, bool overclock, uint16_t mv);
bool tuning_frequency_allowed(const GlobalState * gs, bool overclock, float mhz);

// For consumers that read NVS directly (boot, power management loop): clamp a
// value that may have been persisted before the checks existed. Reads the
// overclock flag from NVS itself and logs once per distinct clamped value.
uint16_t tuning_clamp_voltage(const GlobalState * gs, uint16_t mv, const char * source);
float tuning_clamp_frequency(const GlobalState * gs, float mhz, const char * source);

#endif /* TUNING_LIMITS_H_ */
