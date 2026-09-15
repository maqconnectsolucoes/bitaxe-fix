#ifndef MQTT_TASK_H_
#define MQTT_TASK_H_

#include <stddef.h>

/** Longest device id accepted, matching NVS_CONFIG_MQTT_DEVICE_ID's .max. */
#define MQTT_DEVICE_ID_MAX_LEN 64

/**
 * @brief Resolves this device's MQTT identity.
 *
 * Returns the operator-configured id when one is stored, otherwise derives a
 * unique default from the station MAC ("bitaxe-3cdc755aa5fc"). The MAC is used
 * instead of the hostname because the hostname is user-settable and two boards
 * out of the box answer to the same one - which would collide on the broker.
 */
void mqtt_get_device_id(char *out, size_t out_len);

/** FreeRTOS task: publishes telemetry to the configured broker. */
void mqtt_task(void *pvParameters);

#endif /* MQTT_TASK_H_ */
