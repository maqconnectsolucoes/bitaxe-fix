#include <Arduino.h>
#include <unity.h>
#include <string.h>
#include "metrics.h"

void test_efficiency_is_watts_per_terahash(void)
{
    BitaxeStatus s = {};
    s.power = 17.2f;
    s.hashRate10m = 1204.8f;  // Gh/s = 1.2048 Th/s
    float jth = 0.0f;
    TEST_ASSERT_TRUE(metrics_efficiency(s, jth));
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 14.28f, jth);
}

void test_efficiency_hidden_when_hashrate_below_threshold(void)
{
    BitaxeStatus s = {};
    s.power = 17.2f;
    s.hashRate10m = 0.0f;  // minerador parado: divisão por zero
    float jth = 0.0f;
    TEST_ASSERT_FALSE(metrics_efficiency(s, jth));
}

void test_efficiency_hidden_just_below_one_ghs(void)
{
    BitaxeStatus s = {};
    s.power = 5.0f;
    s.hashRate10m = 0.99f;
    float jth = 0.0f;
    TEST_ASSERT_FALSE(metrics_efficiency(s, jth));
}

void test_freshness_thresholds(void)
{
    TEST_ASSERT_EQUAL(FRESHNESS_NOW, metrics_freshness(0));
    TEST_ASSERT_EQUAL(FRESHNESS_NOW, metrics_freshness(59));
    TEST_ASSERT_EQUAL(FRESHNESS_AGING, metrics_freshness(60));
    TEST_ASSERT_EQUAL(FRESHNESS_AGING, metrics_freshness(599));
    TEST_ASSERT_EQUAL(FRESHNESS_STALE, metrics_freshness(600));
}

void test_hashrate_switches_to_terahash_at_1000(void)
{
    char buf[24];
    metrics_format_hashrate(999.0f, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("999.0 Gh/s", buf);
    metrics_format_hashrate(1000.0f, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("1.00 Th/s", buf);
}

void test_hashrate_parts_switches_to_terahash_at_1000(void)
{
    char value[24];
    char unit[8];

    metrics_format_hashrate_parts(999.0f, value, sizeof(value), unit, sizeof(unit));
    TEST_ASSERT_EQUAL_STRING("999.0", value);
    TEST_ASSERT_EQUAL_STRING("Gh/s", unit);

    metrics_format_hashrate_parts(1000.0f, value, sizeof(value), unit, sizeof(unit));
    TEST_ASSERT_EQUAL_STRING("1.00", value);
    TEST_ASSERT_EQUAL_STRING("Th/s", unit);
}

void setUp(void) {}
void tearDown(void) {}

// Testes no alvo usam setup()/loop(), não main().
void setup()
{
    delay(2000);
    UNITY_BEGIN();
    RUN_TEST(test_efficiency_is_watts_per_terahash);
    RUN_TEST(test_efficiency_hidden_when_hashrate_below_threshold);
    RUN_TEST(test_efficiency_hidden_just_below_one_ghs);
    RUN_TEST(test_freshness_thresholds);
    RUN_TEST(test_hashrate_switches_to_terahash_at_1000);
    RUN_TEST(test_hashrate_parts_switches_to_terahash_at_1000);
    UNITY_END();
}

void loop() {}
