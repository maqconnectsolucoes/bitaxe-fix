#ifndef POWER_MANAGEMENT_TASK_H_
#define POWER_MANAGEMENT_TASK_H_

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    float fan_perc;
    uint16_t fan_rpm;
    uint16_t fan2_rpm;
    float chip_temp_avg;
    float chip_temp2_avg;
    float vr_temp;
    float voltage;
    float frequency_value;
    float actual_frequency;    
    float expected_hashrate;
    float power;
    float current;
    float core_voltage;
    bool is_voltage_throttling;
    bool is_thermal_throttling;
    // Chip temperature sensor liveness: the drivers return -1 both for "not
    // initialised" and for an I2C read failure, so a dead sensor would leave
    // the thermal governors frozen on the last good value forever. Deadline
    // is pushed forward on every valid reading; passing it flags a fault.
    int64_t chip_temp_deadline_us;
    bool temp_sensor_fault;
    // The power sensor (TPS546/INA260) returns its last good value on an I2C
    // failure; set once it has failed enough consecutive reads that voltage /
    // power / current above should be treated as stale.
    bool power_sensor_fault;
} PowerManagementModule;

void POWER_MANAGEMENT_init_frequency(void * pvParameters);

void POWER_MANAGEMENT_task(void * pvParameters);

#endif
