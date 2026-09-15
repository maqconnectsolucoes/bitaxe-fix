#ifndef ASIC_COMMON_H_
#define ASIC_COMMON_H_

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

static const double NONCE_SPACE = 4294967296.0; //  2^32

// Sentinel for task_result.midstate_index meaning "no precomputed midstate
// applies" - always >= bm_job.num_midstates (max 4), so test_nonce_value()
// falls back to hashing the full header instead of matching a real index.
#define MIDSTATE_INDEX_NONE 0xFF

typedef enum
{
    REGISTER_INVALID = 0,
    REGISTER_HASHRATE,       // hashrate register (BM1397)
    REGISTER_TOTAL_COUNT,    // total counter (BM1366,BM1368,BM1370)
    REGISTER_DOMAIN_0_COUNT, // domain counters (BM1366,BM1368,BM1370)
    REGISTER_DOMAIN_1_COUNT,
    REGISTER_DOMAIN_2_COUNT,
    REGISTER_DOMAIN_3_COUNT,
    REGISTER_ERROR_COUNT,    // error count register (all)
    REGISTER_PLL_PARAM,      // PLL/clock config readback (BM1370)
} register_type_t;

typedef struct
{
    // -- job result response
    uint8_t job_id;
    uint32_t nonce;
    uint32_t rolled_version;
    // Index (0-3) of the job->midstate/midstate1/midstate2/midstate3 that
    // rolled_version was derived from, so nonce validation can resume the
    // SHA-256 from that precomputed midstate instead of rehashing 64 bytes.
    // Only BM1397 selects rolled_version from one of those 4 host-precomputed
    // variants; other chips roll version bits directly across the whole
    // mask, so they must set this to MIDSTATE_INDEX_NONE to force the
    // full-header fallback in test_nonce_value() instead of matching index 0
    // by accident (0 is a real, valid index once version-rolling is on).
    uint8_t midstate_index;
    // ---- register response
    register_type_t register_type;
    uint8_t asic_nr;
    uint32_t value;
    uint8_t core_id;
    uint8_t small_core_id;
    // ---- timestamp
    uint64_t timestamp_us;
} task_result;

unsigned char _reverse_bits(unsigned char num);
int _largest_power_of_two(int num);
int _next_power_of_two(int num);
void clear_asic_chain_error(void);
const char *get_asic_chain_error(void);
int count_asic_chips(uint16_t asic_count, uint16_t chip_id, int chip_id_response_length);
esp_err_t receive_work(uint8_t * buffer, int buffer_size, uint64_t *out_timestamp_us);
void get_difficulty_mask(double difficulty, uint8_t *job_difficulty_mask);
double calculate_bm_timeout_ms(float frequency_mhz, size_t asic_count, size_t small_cores, size_t cores, size_t version_size, float timeout_percent, double default_time_ms);

// Highest / lowest value in a 0-terminated preset table (the per-chip
// frequency_options[] / voltage_options[] arrays from device_config.h).
// Returns 0 for a NULL or empty table.
uint16_t asic_options_max(const uint16_t * options);
uint16_t asic_options_min(const uint16_t * options);

typedef struct
{
    uint16_t min;
    uint16_t max;
} asic_range_t;

// Effective [min, max] a tuning write (voltage in mV or frequency in MHz) may
// use. Without overclock it is exactly the chip's preset table. With overclock
// it widens to the chip's absolute oc_min/oc_max ceiling, but never narrows
// below the presets, and an unset ceiling (oc_max == 0) keeps preset-only
// behaviour rather than opening an unbounded range. An empty preset table
// yields {0, 0} (reject everything) unless overclock supplies the range.
asic_range_t asic_tuning_range(const uint16_t * options, uint16_t oc_min, uint16_t oc_max, bool overclock);

#endif /* ASIC_COMMON_H_ */
