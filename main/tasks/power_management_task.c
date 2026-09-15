#include <math.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "global_state.h"
#include "nvs_config.h"
#include "tuning_limits.h"
#include "vcore.h"
#include "thermal.h"
#include "power.h"
#include "asic.h"
#include "utils.h"
#include "asic_init.h"
#include "asic_reset.h"
#include "esp_timer.h"
#include "driver/uart.h"

#define POLL_RATE 100
#define MAX_TEMP 90.0
#define THROTTLE_TEMP 75.0
#define SAFE_TEMP 45.0

#define VOLTAGE_START_THROTTLE 4900
#define VOLTAGE_MIN_THROTTLE 3500
#define VOLTAGE_RANGE (VOLTAGE_START_THROTTLE - VOLTAGE_MIN_THROTTLE)
#define THROTTLE_FLOOR_MHZ 50.0f

#define TPS546_THROTTLE_TEMP 105.0
#define TPS546_MAX_TEMP 145.0

#define ASIC_REDUCTION 100.0

// Retry schedule for mining_start() after a pause/fault clears: 5 s doubling
// up to 60 s, and a sticky hardware_fault after this many failures.
#define MINING_START_BACKOFF_BASE_US (5LL * 1000000)
#define MINING_START_BACKOFF_MAX_US (60LL * 1000000)
#define MINING_START_MAX_FAILURES 3

// Chip temperature sensor liveness: no valid reading for this long while the
// ASIC is up is treated as a fault (the thermal governors would otherwise
// keep running on a frozen value). The grace period covers (re)init.
#define TEMP_SENSOR_TIMEOUT_US (10LL * 1000000)
#define TEMP_SENSOR_GRACE_US (60LL * 1000000)

// Consecutive failed power-sensor reads (3 per 100 ms tick on the TPS546)
// before its cached values are treated as stale.
#define POWER_SENSOR_STALE_READS 50

static const char * TAG = "power_management";

static void mining_stop(GlobalState * GLOBAL_STATE)
{
    ESP_LOGI(TAG, "Stopping mining");

    // Wind frequency down to 50 MHz before cutting power. This also updates
    // the transition tracker so the ramp starts from 50 MHz on next start,
    // rather than the stale pre-reset frequency.
    GLOBAL_STATE->POWER_MANAGEMENT_MODULE.frequency_value = 50;
    GLOBAL_STATE->POWER_MANAGEMENT_MODULE.expected_hashrate = 0;

    ASIC_set_frequency(GLOBAL_STATE);
    ASIC_set_nonce_space(GLOBAL_STATE);

    // Cut ASIC power and hold in reset
    if (VCORE_set_voltage(GLOBAL_STATE, 0.0f) != ESP_OK) {
        ESP_LOGE(TAG, "FATAL: Failed to cut ASIC voltage. Flagging hardware fault.");
        GLOBAL_STATE->SYSTEM_MODULE.hardware_fault = true;
        snprintf(GLOBAL_STATE->SYSTEM_MODULE.hardware_fault_msg, sizeof(GLOBAL_STATE->SYSTEM_MODULE.hardware_fault_msg),
                 "Failed to cut ASIC voltage");
    }
    asic_hold_reset_low();

    // Mark uninitialized immediately so tasks stop issuing UART commands
    GLOBAL_STATE->ASIC_initalized = false;

    // Give tasks time to complete any in-progress UART operation
    vTaskDelay(500 / portTICK_PERIOD_MS);

    // Flush any stale data from the UART buffers
    uart_flush(UART_NUM_1);
    vTaskDelay(100 / portTICK_PERIOD_MS);

    ESP_LOGI(TAG, "Mining stopped");
}

static uint8_t mining_start(GlobalState * GLOBAL_STATE)
{
    ESP_LOGI(TAG, "Starting mining");

    // Restore voltage from NVS
    uint16_t voltage = tuning_clamp_voltage(GLOBAL_STATE, nvs_config_get_u16(NVS_CONFIG_ASIC_VOLTAGE), "mining_start");
    if (VCORE_set_voltage(GLOBAL_STATE, (double) voltage / 1000.0) != ESP_OK) {
        ESP_LOGE(TAG, "FATAL: Failed to set ASIC voltage. Flagging hardware fault.");
        GLOBAL_STATE->SYSTEM_MODULE.hardware_fault = true;
        snprintf(GLOBAL_STATE->SYSTEM_MODULE.hardware_fault_msg, sizeof(GLOBAL_STATE->SYSTEM_MODULE.hardware_fault_msg),
                 "Failed to set ASIC voltage");
        return 0;
    }

    // Wait for voltage to stabilize before touching the ASIC
    vTaskDelay(500 / portTICK_PERIOD_MS);

    // Clear any accumulated UART garbage before init
    uart_flush(UART_NUM_1);
    vTaskDelay(100 / portTICK_PERIOD_MS);

    POWER_MANAGEMENT_init_frequency(GLOBAL_STATE);
    // Stabilization delay of 2000ms prevents race conditions where tasks are
    // just starting to use the ASIC while power management tries to change frequency
    uint8_t chip_count = asic_initialize(GLOBAL_STATE, ASIC_INIT_RECOVERY, 2000);

    if (chip_count > 0) {
        ESP_LOGI(TAG, "Mining started successfully (%d chip(s))", chip_count);
    } else {
        ESP_LOGE(TAG, "Mining start failed - ASIC not detected");
    }

    return chip_count;
}

static float expected_hashrate(GlobalState * GLOBAL_STATE)
{
    return GLOBAL_STATE->POWER_MANAGEMENT_MODULE.frequency_value * GLOBAL_STATE->DEVICE_CONFIG.family.asic.small_core_count * GLOBAL_STATE->DEVICE_CONFIG.family.asic_count / 1000.0;
}

void POWER_MANAGEMENT_init_frequency(void * pvParameters)
{
    GlobalState * GLOBAL_STATE = (GlobalState *) pvParameters;

    float frequency = tuning_clamp_frequency(GLOBAL_STATE, nvs_config_get_float(NVS_CONFIG_ASIC_FREQUENCY), "init_frequency");

    GLOBAL_STATE->POWER_MANAGEMENT_MODULE.frequency_value = frequency;
    GLOBAL_STATE->POWER_MANAGEMENT_MODULE.actual_frequency = 50.0;
    GLOBAL_STATE->POWER_MANAGEMENT_MODULE.expected_hashrate = expected_hashrate(GLOBAL_STATE);
    
    char expected_hashrate_str[16] = {0};
    suffixString(GLOBAL_STATE->POWER_MANAGEMENT_MODULE.expected_hashrate * 1e6, expected_hashrate_str, sizeof(expected_hashrate_str), 0);
    ESP_LOGI(TAG, "ASIC Frequency: %g MHz, Expected hashrate: %sH/s", frequency, expected_hashrate_str);
}

void POWER_MANAGEMENT_task(void * pvParameters)
{
    ESP_LOGI(TAG, "Starting");

    GlobalState * GLOBAL_STATE = (GlobalState *) pvParameters;

    PowerManagementModule * power_management = &GLOBAL_STATE->POWER_MANAGEMENT_MODULE;
    SystemModule * sys_module = &GLOBAL_STATE->SYSTEM_MODULE;

    POWER_MANAGEMENT_init_frequency(GLOBAL_STATE);
    
    float last_asic_frequency = power_management->frequency_value;

    vTaskDelay(500 / portTICK_PERIOD_MS);
    uint16_t last_core_voltage = 0.0;

    uint16_t last_known_asic_voltage = 0;
    float last_known_asic_frequency = 0.0;
    bool is_paused = false;
    uint32_t start_failures = 0;
    int64_t next_start_attempt_us = 0;
    bool last_asic_initalized = false;

    // Input-voltage throttle state: filtered_input_voltage smooths out ADC/I2C sample
    // noise so brief jitter near a threshold doesn't reconfigure the ASIC PLL every tick.
    float filtered_input_voltage = -1.0f;
    bool is_voltage_throttling = false;

    // Gradual thermal throttle state: same EMA-filter shape, applied to chip/VR temps.
    float filtered_chip_temp = -1.0f;
    float filtered_vr_temp = -1.0f;
    bool is_thermal_throttling = false;

    power_management->is_voltage_throttling = false;
    power_management->is_thermal_throttling = false;
    power_management->chip_temp_deadline_us = esp_timer_get_time() + TEMP_SENSOR_GRACE_US;
    power_management->temp_sensor_fault = false;

    while (1) {
        if (GLOBAL_STATE->SELF_TEST_MODULE.is_finished) {
            ESP_LOGI(TAG, "Stopped");
            vTaskDelete(NULL);
            return;
        }

        power_management->voltage = Power_get_input_voltage(GLOBAL_STATE);
        Power_get_output(GLOBAL_STATE, &power_management->power, &power_management->current);
        power_management->core_voltage = VCORE_get_voltage_mv(GLOBAL_STATE);

        power_management->chip_temp_avg = Thermal_get_chip_temp(GLOBAL_STATE);
        power_management->chip_temp2_avg = Thermal_get_chip_temp2(GLOBAL_STATE);

        power_management->vr_temp = Power_get_vreg_temp(GLOBAL_STATE);

        // Sensor liveness. The thermal drivers return -1 both for "ASIC not
        // initialised" and for an I2C read failure, and the governors below
        // simply skip non-positive readings - so a sensor that dies mid-run
        // would leave filtered_chip_temp frozen on its last good value and
        // asic_overheat (-1 > MAX_TEMP) never firing. With no temperature
        // the only safe state is stopped, so it becomes a hardware fault.
        {
            int64_t now_us = esp_timer_get_time();
            bool asic_up = GLOBAL_STATE->ASIC_initalized;
            if (asic_up && !last_asic_initalized) {
                power_management->chip_temp_deadline_us = now_us + TEMP_SENSOR_GRACE_US;
            }
            last_asic_initalized = asic_up;

            if (power_management->chip_temp_avg > 0 || power_management->chip_temp2_avg > 0) {
                power_management->chip_temp_deadline_us = now_us + TEMP_SENSOR_TIMEOUT_US;
                power_management->temp_sensor_fault = false;
            } else if (asic_up && !is_paused && !power_management->temp_sensor_fault &&
                       now_us > power_management->chip_temp_deadline_us) {
                power_management->temp_sensor_fault = true;
                ESP_LOGE(TAG, "Chip temperature unreadable for %lld s - thermal protection is blind, stopping mining",
                         (long long) (TEMP_SENSOR_TIMEOUT_US / 1000000));
                if (!sys_module->hardware_fault) {
                    sys_module->hardware_fault = true;
                    snprintf(sys_module->hardware_fault_msg, sizeof(sys_module->hardware_fault_msg),
                             "Chip temperature sensor unreadable");
                }
            }
        }

        // Power sensor staleness: TPS546/INA260 hand back their last good value
        // on an I2C failure, so without this the input-voltage throttle would
        // keep acting on a VIN frozen at some past moment. The ESP32 brownout
        // detector remains the backstop while readings are stale.
        {
            bool stale = Power_get_read_failures(GLOBAL_STATE) >= POWER_SENSOR_STALE_READS;
            if (stale != power_management->power_sensor_fault) {
                power_management->power_sensor_fault = stale;
                if (stale) {
                    ESP_LOGW(TAG, "Power sensor unreadable for %d consecutive reads - input-voltage throttle suspended",
                             POWER_SENSOR_STALE_READS);
                    filtered_input_voltage = -1.0f; // reseed from a fresh sample on recovery
                } else {
                    ESP_LOGI(TAG, "Power sensor readings recovered");
                }
            }
        }

        // User pause, hardware fault, or all pools unreachable
        bool wants_stop = sys_module->mining_paused || sys_module->hardware_fault || sys_module->pools_unavailable;
        if (wants_stop && !is_paused) {
            mining_stop(GLOBAL_STATE);
            is_paused = true;
            start_failures = 0;
            next_start_attempt_us = 0;
        } else if (!wants_stop && is_paused && esp_timer_get_time() >= next_start_attempt_us) {
            // mining_start() returns the chip count; 0 means the ASIC did not
            // come back (UART garbage, chip stuck after the power cut). Stay
            // paused and retry with backoff rather than declaring it running
            // with Vcore applied to a chip in reset and no thermal readings.
            if (mining_start(GLOBAL_STATE) > 0) {
                is_paused = false;
                start_failures = 0;
            } else {
                start_failures++;
                int64_t backoff_us = MINING_START_BACKOFF_BASE_US << (start_failures < 4 ? start_failures : 4);
                if (backoff_us > MINING_START_BACKOFF_MAX_US) {
                    backoff_us = MINING_START_BACKOFF_MAX_US;
                }
                next_start_attempt_us = esp_timer_get_time() + backoff_us;
                ESP_LOGE(TAG, "Mining start attempt %lu failed, retrying in %lld s", (unsigned long) start_failures,
                         (long long) (backoff_us / 1000000));
                if (start_failures >= MINING_START_MAX_FAILURES && !sys_module->hardware_fault) {
                    sys_module->hardware_fault = true;
                    snprintf(sys_module->hardware_fault_msg, sizeof(sys_module->hardware_fault_msg),
                             "ASIC not detected after %lu restart attempts", (unsigned long) start_failures);
                }
            }
        }

        // If we've paused or have a hardware fault, skip doing anything else
        if (is_paused || sys_module->hardware_fault) {
            vTaskDelay(POLL_RATE / portTICK_PERIOD_MS);
            continue;
        }

        // Hard emergency stop - last-resort backstop for when the gradual thermal
        // throttle below wasn't enough (fan failure, blocked airflow, thermal runaway).
        // Under normal operation the gradual throttle should keep temps from ever
        // reaching these absolute limits.
        bool asic_overheat =
            power_management->chip_temp_avg > MAX_TEMP
            || power_management->chip_temp2_avg > MAX_TEMP;

        if ((power_management->vr_temp > TPS546_MAX_TEMP || asic_overheat) && (power_management->frequency_value > 50 || power_management->voltage > 1000)) {
            if (power_management->chip_temp2_avg > 0) {
                ESP_LOGE(TAG, "OVERHEAT! VR: %fC ASIC1: %fC ASIC2: %fC", power_management->vr_temp, power_management->chip_temp_avg, power_management->chip_temp2_avg);
            } else {
                ESP_LOGE(TAG, "OVERHEAT! VR: %fC ASIC: %fC", power_management->vr_temp, power_management->chip_temp_avg);
            }

            last_known_asic_voltage = nvs_config_get_u16(NVS_CONFIG_ASIC_VOLTAGE);
            last_known_asic_frequency = nvs_config_get_float(NVS_CONFIG_ASIC_FREQUENCY);
            nvs_config_set_bool(NVS_CONFIG_AUTO_FAN_SPEED, false);
            nvs_config_set_u16(NVS_CONFIG_MANUAL_FAN_SPEED, 100);
            nvs_config_set_bool(NVS_CONFIG_OVERHEAT_MODE, true);
            ESP_LOGW(TAG, "Entering safe mode due to overheat condition. System operation halted.");
            mining_stop(GLOBAL_STATE);
            
            // Note: ASIC temperature readings are invalid when ASIC is powered down (returns -1)
            // For 600-series boards that use ASIC thermal diode, we rely on VR temp and fixed cooling time
            // For boards with EMC internal temp sensor, readings remain valid
            bool asic_temp_valid = GLOBAL_STATE->DEVICE_CONFIG.emc_internal_temp;
            int cooling_cycles = 0;
            const int MIN_COOLING_CYCLES = 6; // Minimum 30 seconds cooling
            
            while (cooling_cycles < MIN_COOLING_CYCLES || power_management->vr_temp > TPS546_THROTTLE_TEMP - 10) {
                vTaskDelay(5000 / portTICK_PERIOD_MS); // Wait 5 seconds
                cooling_cycles++;
                
                power_management->vr_temp = Power_get_vreg_temp(GLOBAL_STATE);
                
                // Only check ASIC temps if they're valid (not using ASIC thermal diode)
                if (asic_temp_valid) {
                    power_management->chip_temp_avg = Thermal_get_chip_temp(GLOBAL_STATE);
                    power_management->chip_temp2_avg = Thermal_get_chip_temp2(GLOBAL_STATE);
                    ESP_LOGW(TAG, "Safe mode active (cycle %d) - VR: %.1f°C ASIC1: %.1f°C ASIC2: %.1f°C",
                             cooling_cycles, power_management->vr_temp, power_management->chip_temp_avg, power_management->chip_temp2_avg);
                    
                    // Continue if ASIC temps still too high
                    if (power_management->chip_temp_avg >  SAFE_TEMP || power_management->chip_temp2_avg > SAFE_TEMP) {
                        cooling_cycles = 0; // Reset cycle count if still hot
                    }
                } else {
                    // For boards using ASIC thermal diode (600 series), rely on VR temp and time
                    ESP_LOGW(TAG, "Safe mode active (cycle %d/%d) - VR: %.1f°C (ASIC temps unavailable while powered down)",
                             cooling_cycles, MIN_COOLING_CYCLES, power_management->vr_temp);
                }
            }
            ESP_LOGI(TAG, "Temperature normalized after %d cooling cycles. Reinitializing ASIC...", cooling_cycles);
            
            uint16_t reduced_voltage = last_known_asic_voltage > ASIC_REDUCTION ? last_known_asic_voltage - ASIC_REDUCTION : 1000;
            float reduced_asic_frequency = last_known_asic_frequency > ASIC_REDUCTION ? last_known_asic_frequency - ASIC_REDUCTION : 400.0;
            
            nvs_config_set_u16(NVS_CONFIG_ASIC_VOLTAGE, reduced_voltage);
            nvs_config_set_float(NVS_CONFIG_ASIC_FREQUENCY, reduced_asic_frequency);
            
            ESP_LOGI(TAG, "Restoring at reduced settings: %umV (was %umV), %.0f MHz (was %.0f MHz)",
                     reduced_voltage, last_known_asic_voltage, reduced_asic_frequency, last_known_asic_frequency);

            uint8_t chip_count = mining_start(GLOBAL_STATE);

            if (chip_count > 0) {
                // Frequency reduction will now be applied by normal power management loop
                nvs_config_set_bool(NVS_CONFIG_OVERHEAT_MODE, false);
                ESP_LOGI(TAG, "Resuming normal operation. Reduced frequency (%.0f MHz) will be applied automatically.", reduced_asic_frequency);
            }
        }

        uint16_t core_voltage = GLOBAL_STATE->SELF_TEST_MODULE.is_active
                                 ? GLOBAL_STATE->DEVICE_CONFIG.family.asic.default_voltage_mv
                                 : tuning_clamp_voltage(GLOBAL_STATE, nvs_config_get_u16(NVS_CONFIG_ASIC_VOLTAGE), "power_management");
        float requested_asic_frequency = GLOBAL_STATE->SELF_TEST_MODULE.is_active
                                 ? GLOBAL_STATE-> DEVICE_CONFIG.family.asic.default_frequency_mhz
                                 : tuning_clamp_frequency(GLOBAL_STATE, nvs_config_get_float(NVS_CONFIG_ASIC_FREQUENCY), "power_management");

        // Input-voltage throttle: proactively reduce ASIC frequency (and so current draw)
        // as the input rail sags, instead of only reacting after a hardware brownout reset
        // trips. A voltage of 0 means this board has no input-voltage sensor - don't throttle.
        // While the power sensor is stale, don't feed its frozen value into the filter either.
        if (power_management->voltage > 0 && !power_management->power_sensor_fault) {
            filtered_input_voltage = (filtered_input_voltage < 0)
                ? power_management->voltage
                : (0.2f * power_management->voltage) + (0.8f * filtered_input_voltage);
        }

        float voltage_frequency = requested_asic_frequency;
        if (!power_management->power_sensor_fault && filtered_input_voltage > 0 &&
            filtered_input_voltage < VOLTAGE_START_THROTTLE) {
            float throttle_fraction = (filtered_input_voltage <= VOLTAGE_MIN_THROTTLE)
                ? 0.0f
                : (filtered_input_voltage - VOLTAGE_MIN_THROTTLE) / (float) VOLTAGE_RANGE;
            float freq_floor = fminf(THROTTLE_FLOOR_MHZ, requested_asic_frequency);
            voltage_frequency = freq_floor + (requested_asic_frequency - freq_floor) * throttle_fraction;
        }

        // Gradual thermal throttle: derate frequency continuously between THROTTLE_TEMP
        // (start) and MAX_TEMP (floor) - and likewise for the regulator between
        // TPS546_THROTTLE_TEMP and TPS546_MAX_TEMP - so the ASIC settles at whatever
        // frequency it can sustain instead of running flat-out until the hard emergency
        // stop above trips. Takes the worse of the two chip sensors, same as asic_overheat.
        float chip_temp_max = -1.0f;
        if (power_management->chip_temp_avg > chip_temp_max) chip_temp_max = power_management->chip_temp_avg;
        if (power_management->chip_temp2_avg > chip_temp_max) chip_temp_max = power_management->chip_temp2_avg;

        if (chip_temp_max > 0) {
            filtered_chip_temp = (filtered_chip_temp < 0)
                ? chip_temp_max
                : (0.2f * chip_temp_max) + (0.8f * filtered_chip_temp);
        }
        if (power_management->vr_temp > 0) {
            filtered_vr_temp = (filtered_vr_temp < 0)
                ? power_management->vr_temp
                : (0.2f * power_management->vr_temp) + (0.8f * filtered_vr_temp);
        }

        float chip_throttle_fraction = 1.0f;
        if (filtered_chip_temp > THROTTLE_TEMP) {
            chip_throttle_fraction = (filtered_chip_temp >= MAX_TEMP)
                ? 0.0f
                : 1.0f - (filtered_chip_temp - THROTTLE_TEMP) / (float) (MAX_TEMP - THROTTLE_TEMP);
        }
        float vr_throttle_fraction = 1.0f;
        if (filtered_vr_temp > TPS546_THROTTLE_TEMP) {
            vr_throttle_fraction = (filtered_vr_temp >= TPS546_MAX_TEMP)
                ? 0.0f
                : 1.0f - (filtered_vr_temp - TPS546_THROTTLE_TEMP) / (float) (TPS546_MAX_TEMP - TPS546_THROTTLE_TEMP);
        }
        float thermal_throttle_fraction = fminf(chip_throttle_fraction, vr_throttle_fraction);

        float thermal_frequency = requested_asic_frequency;
        if (thermal_throttle_fraction < 1.0f) {
            float freq_floor = fminf(THROTTLE_FLOOR_MHZ, requested_asic_frequency);
            thermal_frequency = freq_floor + (requested_asic_frequency - freq_floor) * thermal_throttle_fraction;
        }

        // Combine: whichever governor is more restrictive wins. Round once, to the
        // nearest 5 MHz, so residual filter noise near a threshold doesn't still
        // trigger a PLL reconfiguration on every poll tick.
        float asic_frequency = roundf(fminf(voltage_frequency, thermal_frequency) / 5.0f) * 5.0f;

        bool now_voltage_throttling = voltage_frequency < requested_asic_frequency;
        power_management->is_voltage_throttling = now_voltage_throttling;
        if (now_voltage_throttling != is_voltage_throttling) {
            is_voltage_throttling = now_voltage_throttling;
            if (is_voltage_throttling) {
                ESP_LOGW(TAG, "Input voltage low (%.0fmV): throttling ASIC frequency toward %.0f MHz (requested %.0f MHz)",
                         filtered_input_voltage, voltage_frequency, requested_asic_frequency);
            } else {
                ESP_LOGI(TAG, "Input voltage recovered: no longer throttling for voltage");
            }
        }

        bool now_thermal_throttling = thermal_throttle_fraction < 1.0f;
        power_management->is_thermal_throttling = now_thermal_throttling;
        if (now_thermal_throttling != is_thermal_throttling) {
            is_thermal_throttling = now_thermal_throttling;
            if (is_thermal_throttling) {
                ESP_LOGW(TAG, "Chip/VR temp approaching limit (chip %.1fC, VR %.1fC): throttling ASIC frequency toward %.0f MHz (requested %.0f MHz)",
                         filtered_chip_temp, filtered_vr_temp, thermal_frequency, requested_asic_frequency);
            } else {
                ESP_LOGI(TAG, "Chip/VR temp recovered: no longer throttling for temperature");
            }
        }

        if (core_voltage != last_core_voltage) {
            ESP_LOGI(TAG, "setting new vcore voltage to %umV", core_voltage);
            if (VCORE_set_voltage(GLOBAL_STATE, (double) core_voltage / 1000.0) != ESP_OK) {
                ESP_LOGE(TAG, "FATAL: Failed to set ASIC voltage. Flagging hardware fault.");
                GLOBAL_STATE->SYSTEM_MODULE.hardware_fault = true;
                snprintf(GLOBAL_STATE->SYSTEM_MODULE.hardware_fault_msg, sizeof(GLOBAL_STATE->SYSTEM_MODULE.hardware_fault_msg),
                         "Failed to set ASIC voltage");
                // Don't update last_core_voltage: retry next loop, and don't proceed to
                // raise ASIC frequency below believing voltage was actually applied.
            } else {
                last_core_voltage = core_voltage;
            }
        }

        if (asic_frequency != last_asic_frequency) {
            ESP_LOGI(TAG, "New ASIC frequency requested: %g MHz (current: %g MHz)", asic_frequency, last_asic_frequency);
            
            power_management->frequency_value = asic_frequency;
            power_management->expected_hashrate = expected_hashrate(GLOBAL_STATE);

            ASIC_set_frequency(GLOBAL_STATE);
            ASIC_set_nonce_space(GLOBAL_STATE);
            
            last_asic_frequency = asic_frequency;
        }

        // Check for changing of overheat mode
        bool new_overheat_mode = nvs_config_get_bool(NVS_CONFIG_OVERHEAT_MODE);
        
        if (new_overheat_mode != sys_module->overheat_mode) {
            sys_module->overheat_mode = new_overheat_mode;
            ESP_LOGI(TAG, "Overheat mode updated to: %d", sys_module->overheat_mode);
        }

        VCORE_check_fault(GLOBAL_STATE);

        // looper:
        vTaskDelay(POLL_RATE / portTICK_PERIOD_MS);
    }
}
