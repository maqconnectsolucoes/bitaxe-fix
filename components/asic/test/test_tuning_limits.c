#include "unity.h"

#include "asic_common.h"

// Mirrors BM1370_FREQUENCY_OPTIONS / BM1370_VOLTAGE_OPTIONS from device_config.h:
// ascending, 0-terminated preset tables.
static const uint16_t BM1370_FREQ[] = {400, 490, 525, 550, 600, 625, 0};
static const uint16_t BM1370_VOLT[] = {1000, 1060, 1100, 1150, 1200, 1250, 0};

TEST_CASE("asic_options_max returns the highest preset", "[common]")
{
    TEST_ASSERT_EQUAL_UINT16(625, asic_options_max(BM1370_FREQ));
    TEST_ASSERT_EQUAL_UINT16(1250, asic_options_max(BM1370_VOLT));
}

TEST_CASE("asic_options_min returns the lowest preset", "[common]")
{
    TEST_ASSERT_EQUAL_UINT16(400, asic_options_min(BM1370_FREQ));
    TEST_ASSERT_EQUAL_UINT16(1000, asic_options_min(BM1370_VOLT));
}

TEST_CASE("asic_options_max/min do not assume the table is sorted", "[common]")
{
    static const uint16_t unsorted[] = {550, 400, 625, 490, 0};
    TEST_ASSERT_EQUAL_UINT16(625, asic_options_max(unsorted));
    TEST_ASSERT_EQUAL_UINT16(400, asic_options_min(unsorted));
}

TEST_CASE("asic_options_max/min on an empty table return 0", "[common]")
{
    static const uint16_t empty[] = {0};
    TEST_ASSERT_EQUAL_UINT16(0, asic_options_max(empty));
    TEST_ASSERT_EQUAL_UINT16(0, asic_options_min(empty));
}

TEST_CASE("asic_options_max/min tolerate a NULL table", "[common]")
{
    TEST_ASSERT_EQUAL_UINT16(0, asic_options_max(NULL));
    TEST_ASSERT_EQUAL_UINT16(0, asic_options_min(NULL));
}

// --- asic_tuning_range: the effective [min, max] a tuning write may use ---

TEST_CASE("tuning range without overclock is bounded by the preset table", "[common]")
{
    asic_range_t r = asic_tuning_range(BM1370_VOLT, 1000, 1350, false);
    TEST_ASSERT_EQUAL_UINT16(1000, r.min);
    TEST_ASSERT_EQUAL_UINT16(1250, r.max);
}

TEST_CASE("tuning range with overclock widens to the absolute ceiling", "[common]")
{
    asic_range_t r = asic_tuning_range(BM1370_VOLT, 1000, 1350, true);
    TEST_ASSERT_EQUAL_UINT16(1000, r.min);
    TEST_ASSERT_EQUAL_UINT16(1350, r.max);
}

TEST_CASE("tuning range with overclock is never narrower than the presets", "[common]")
{
    // A misconfigured absolute ceiling below the preset max must not lock the
    // user out of values the preset dropdown already offers.
    asic_range_t r = asic_tuning_range(BM1370_VOLT, 1100, 1200, true);
    TEST_ASSERT_EQUAL_UINT16(1000, r.min);
    TEST_ASSERT_EQUAL_UINT16(1250, r.max);
}

TEST_CASE("tuning range ignores an unset (0) absolute ceiling", "[common]")
{
    // A chip entry without oc_* fields keeps preset-only behaviour even with
    // overclock on, instead of opening an unbounded range.
    asic_range_t r = asic_tuning_range(BM1370_VOLT, 0, 0, true);
    TEST_ASSERT_EQUAL_UINT16(1000, r.min);
    TEST_ASSERT_EQUAL_UINT16(1250, r.max);
}

TEST_CASE("tuning range on an empty preset table rejects everything unless overclocked", "[common]")
{
    static const uint16_t empty[] = {0};
    asic_range_t off = asic_tuning_range(empty, 1000, 1350, false);
    TEST_ASSERT_EQUAL_UINT16(0, off.min);
    TEST_ASSERT_EQUAL_UINT16(0, off.max);

    asic_range_t on = asic_tuning_range(empty, 1000, 1350, true);
    TEST_ASSERT_EQUAL_UINT16(1000, on.min);
    TEST_ASSERT_EQUAL_UINT16(1350, on.max);
}
