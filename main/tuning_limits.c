#include "tuning_limits.h"

#include <math.h>
#include "esp_log.h"
#include "nvs_config.h"

static const char * TAG = "tuning_limits";

asic_range_t tuning_voltage_range(const GlobalState * gs, bool overclock)
{
    const AsicConfig * asic = &gs->DEVICE_CONFIG.family.asic;
    return asic_tuning_range(asic->voltage_options, asic->oc_min_voltage_mv, asic->oc_max_voltage_mv, overclock);
}

asic_range_t tuning_frequency_range(const GlobalState * gs, bool overclock)
{
    const AsicConfig * asic = &gs->DEVICE_CONFIG.family.asic;
    return asic_tuning_range(asic->frequency_options, asic->oc_min_frequency_mhz, asic->oc_max_frequency_mhz, overclock);
}

bool tuning_voltage_allowed(const GlobalState * gs, bool overclock, uint16_t mv)
{
    asic_range_t r = tuning_voltage_range(gs, overclock);
    return mv >= r.min && mv <= r.max;
}

bool tuning_frequency_allowed(const GlobalState * gs, bool overclock, float mhz)
{
    asic_range_t r = tuning_frequency_range(gs, overclock);
    return mhz >= (float) r.min && mhz <= (float) r.max;
}

uint16_t tuning_clamp_voltage(const GlobalState * gs, uint16_t mv, const char * source)
{
    static uint16_t last_warned_mv = 0;

    asic_range_t r = tuning_voltage_range(gs, nvs_config_get_bool(NVS_CONFIG_OVERCLOCK_ENABLED));
    uint16_t clamped = mv < r.min ? r.min : (mv > r.max ? r.max : mv);

    if (clamped != mv && mv != last_warned_mv) {
        ESP_LOGW(TAG, "%s: core voltage %u mV is outside the %s envelope [%u, %u] mV, clamping to %u mV", source, mv,
                 gs->DEVICE_CONFIG.family.asic.name, r.min, r.max, clamped);
        last_warned_mv = mv;
        // Every caller reads this same key, so persist the corrected value:
        // otherwise /api/system/info keeps reporting the poisoned number while
        // the chip actually runs the clamped one. No-op when unchanged.
        nvs_config_set_u16(NVS_CONFIG_ASIC_VOLTAGE, clamped);
    }
    return clamped;
}

float tuning_clamp_frequency(const GlobalState * gs, float mhz, const char * source)
{
    static float last_warned_mhz = 0.0f;

    asic_range_t r = tuning_frequency_range(gs, nvs_config_get_bool(NVS_CONFIG_OVERCLOCK_ENABLED));
    float clamped = fminf(fmaxf(mhz, (float) r.min), (float) r.max);

    if (clamped != mhz && mhz != last_warned_mhz) {
        ESP_LOGW(TAG, "%s: frequency %.0f MHz is outside the %s envelope [%u, %u] MHz, clamping to %.0f MHz", source, mhz,
                 gs->DEVICE_CONFIG.family.asic.name, r.min, r.max, clamped);
        last_warned_mhz = mhz;
        nvs_config_set_float(NVS_CONFIG_ASIC_FREQUENCY, clamped);
    }
    return clamped;
}
