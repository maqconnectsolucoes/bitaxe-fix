#include <math.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "asic_common.h"
#include "global_state.h"
#include "nvs_config.h"
#include "autotune_task.h"

// Frequency-only autotune: gradually walks the ASIC's own vetted frequency_options[]
// preset ladder upward while the hardware error rate stays low and neither of
// power_management_task's throttle governors is already active, backing off one
// preset (with a cooldown) the moment either signal turns bad. Voltage is never
// touched here - that stays under the user's manual control.

#define AUTOTUNE_POLL_RATE_MS 1000
#define AUTOTUNE_WARMUP_MS (5 * 60 * 1000)          // let filters/averages settle before the first climb
#define AUTOTUNE_CLIMB_INTERVAL_MS (10 * 60 * 1000) // minimum time between successful climbs
#define AUTOTUNE_BACKOFF_COOLDOWN_MS (60 * 1000)    // mandatory pause after backing off before retrying
#define AUTOTUNE_ERROR_STABLE_THRESHOLD 1.0f        // error % below which a sample counts toward "stable"
#define AUTOTUNE_ERROR_UNSTABLE_THRESHOLD 3.0f      // error % that triggers an immediate step down
#define AUTOTUNE_STABLE_WINDOW_SAMPLES 3            // consecutive good samples required just before climbing

static const char * TAG = "autotune";

typedef enum {
    AUTOTUNE_STATE_WARMUP,
    AUTOTUNE_STATE_CLIMBING,
    AUTOTUNE_STATE_BACKOFF,
} autotune_state_t;

// Returns the index of the highest preset <= frequency, or -1 if frequency is
// below every preset. Used instead of an exact-match lookup so autotune keeps
// working even if the current frequency isn't one of the presets (e.g. the
// user typed a custom value while overclock unlock mode is on).
static int find_frequency_floor_index(const uint16_t * frequency_options, uint16_t frequency)
{
    int idx = -1;
    for (int i = 0; frequency_options[i] != 0; i++) {
        if (frequency_options[i] <= frequency) {
            idx = i;
        } else {
            break;
        }
    }
    return idx;
}

// On enabling autotune (including after a reboot with it already on), resume from
// the last confirmed-stable frequency instead of always restarting the search from
// the chip's default. Clamps to the current chip's ceiling in case of a chip swap.
static void resume_from_persisted_frequency(GlobalState * GLOBAL_STATE)
{
    const uint16_t * frequency_options = GLOBAL_STATE->DEVICE_CONFIG.family.asic.frequency_options;
    uint16_t ceiling = asic_options_max(frequency_options);

    float persisted = nvs_config_get_float(NVS_CONFIG_AUTOTUNE_FREQUENCY);
    if (persisted <= 0.0f) {
        return; // never tuned before on this device
    }

    if (persisted > (float) ceiling) {
        ESP_LOGW(TAG, "Persisted autotune frequency %.0f MHz exceeds this chip's ceiling (%u MHz), clamping",
                 persisted, ceiling);
        persisted = (float) ceiling;
        nvs_config_set_float(NVS_CONFIG_AUTOTUNE_FREQUENCY, persisted);
    }

    float current = nvs_config_get_float(NVS_CONFIG_ASIC_FREQUENCY);
    if (persisted > current) {
        ESP_LOGI(TAG, "Resuming from last known-stable frequency: %.0f MHz", persisted);
        nvs_config_set_float(NVS_CONFIG_ASIC_FREQUENCY, persisted);
    }
}

static bool step_frequency_up(GlobalState * GLOBAL_STATE)
{
    const uint16_t * frequency_options = GLOBAL_STATE->DEVICE_CONFIG.family.asic.frequency_options;
    float current = nvs_config_get_float(NVS_CONFIG_ASIC_FREQUENCY);

    int idx = find_frequency_floor_index(frequency_options, (uint16_t) roundf(current));
    if (idx < 0) {
        idx = 0; // current is below the lowest preset - start the climb from the bottom
    } else if (frequency_options[idx + 1] == 0) {
        ESP_LOGI(TAG, "Already at the frequency ceiling (%u MHz), nothing higher to try", frequency_options[idx]);
        return false;
    } else {
        idx = idx + 1;
    }

    uint16_t next_freq = frequency_options[idx];
    ESP_LOGI(TAG, "Stepping frequency up: %.0f -> %u MHz", current, next_freq);
    nvs_config_set_float(NVS_CONFIG_ASIC_FREQUENCY, (float) next_freq);
    nvs_config_set_float(NVS_CONFIG_AUTOTUNE_FREQUENCY, (float) next_freq);
    return true;
}

static bool step_frequency_down(GlobalState * GLOBAL_STATE)
{
    const uint16_t * frequency_options = GLOBAL_STATE->DEVICE_CONFIG.family.asic.frequency_options;
    float current = nvs_config_get_float(NVS_CONFIG_ASIC_FREQUENCY);

    int idx = find_frequency_floor_index(frequency_options, (uint16_t) roundf(current));
    if (idx <= 0) {
        ESP_LOGW(TAG, "Already at the lowest frequency preset, nothing safer to fall back to");
        return false;
    }

    uint16_t prev_freq = frequency_options[idx - 1];
    ESP_LOGW(TAG, "Instability detected (error rate too high): stepping frequency down %.0f -> %u MHz", current, prev_freq);
    nvs_config_set_float(NVS_CONFIG_ASIC_FREQUENCY, (float) prev_freq);
    nvs_config_set_float(NVS_CONFIG_AUTOTUNE_FREQUENCY, (float) prev_freq);
    return true;
}

void autotune_task(void * pvParameters)
{
    GlobalState * GLOBAL_STATE = (GlobalState *) pvParameters;

    ESP_LOGI(TAG, "Starting");

    autotune_state_t state = AUTOTUNE_STATE_WARMUP;
    uint64_t state_entered_at_ms = 0;
    uint64_t last_climb_at_ms = 0;
    int stable_samples = 0;
    bool was_enabled = false;

    while (1) {
        vTaskDelay(AUTOTUNE_POLL_RATE_MS / portTICK_PERIOD_MS);

        bool enabled = nvs_config_get_bool(NVS_CONFIG_AUTOTUNE_ENABLED);
        uint64_t now_ms = (uint64_t) (esp_timer_get_time() / 1000);

        if (!enabled) {
            was_enabled = false;
            continue;
        }

        if (!was_enabled) {
            ESP_LOGI(TAG, "Autotune enabled");
            resume_from_persisted_frequency(GLOBAL_STATE);
            state = AUTOTUNE_STATE_WARMUP;
            state_entered_at_ms = now_ms;
            stable_samples = 0;
            was_enabled = true;
        }

        // Don't act while a factory self-test owns the ASIC, or before it's initialized.
        if (GLOBAL_STATE->SELF_TEST_MODULE.is_active || !GLOBAL_STATE->ASIC_initalized) {
            continue;
        }

        PowerManagementModule * power_management = &GLOBAL_STATE->POWER_MANAGEMENT_MODULE;
        bool governor_active = power_management->is_voltage_throttling || power_management->is_thermal_throttling;
        float error_percentage = GLOBAL_STATE->SYSTEM_MODULE.error_percentage;

        switch (state) {
            case AUTOTUNE_STATE_WARMUP:
                if (now_ms - state_entered_at_ms >= AUTOTUNE_WARMUP_MS) {
                    ESP_LOGI(TAG, "Warmup complete, beginning frequency search");
                    state = AUTOTUNE_STATE_CLIMBING;
                    last_climb_at_ms = now_ms;
                    stable_samples = 0;
                }
                break;

            case AUTOTUNE_STATE_BACKOFF:
                if (now_ms - state_entered_at_ms >= AUTOTUNE_BACKOFF_COOLDOWN_MS) {
                    state = AUTOTUNE_STATE_CLIMBING;
                    last_climb_at_ms = now_ms;
                    stable_samples = 0;
                }
                break;

            case AUTOTUNE_STATE_CLIMBING:
                if (error_percentage >= AUTOTUNE_ERROR_UNSTABLE_THRESHOLD) {
                    step_frequency_down(GLOBAL_STATE);
                    state = AUTOTUNE_STATE_BACKOFF;
                    state_entered_at_ms = now_ms;
                    stable_samples = 0;
                    break;
                }

                if (!governor_active && error_percentage < AUTOTUNE_ERROR_STABLE_THRESHOLD) {
                    if (stable_samples < AUTOTUNE_STABLE_WINDOW_SAMPLES) {
                        stable_samples++;
                    }
                } else {
                    stable_samples = 0;
                }

                if (stable_samples >= AUTOTUNE_STABLE_WINDOW_SAMPLES && (now_ms - last_climb_at_ms) >= AUTOTUNE_CLIMB_INTERVAL_MS) {
                    if (step_frequency_up(GLOBAL_STATE)) {
                        stable_samples = 0;
                    }
                    last_climb_at_ms = now_ms;
                }
                break;
        }
    }
}
