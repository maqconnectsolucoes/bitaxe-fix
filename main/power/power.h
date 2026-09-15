#ifndef POWER_H
#define POWER_H

#include <esp_err.h>
#include "global_state.h"

void Power_get_output(GlobalState * GLOBAL_STATE, float * power_out, float * current_out);
float Power_get_input_voltage(GlobalState * GLOBAL_STATE);
float Power_get_vreg_temp(GlobalState * GLOBAL_STATE);
// Consecutive failed reads on the active power sensor (0 = the values above
// are fresh). The getters return their last good value on failure, so this is
// the only way to know they are stale.
uint32_t Power_get_read_failures(GlobalState * GLOBAL_STATE);

#endif // POWER_H
