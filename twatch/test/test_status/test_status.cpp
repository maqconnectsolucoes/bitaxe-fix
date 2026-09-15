#include <Arduino.h>
#include <unity.h>
#include <type_traits>
#include "bitaxe_status.h"

void test_status_is_pod_so_it_survives_deep_sleep(void)
{
    // RTC_DATA_ATTR exige POD: String ou ponteiro aqui viraria lixo no despertar.
    TEST_ASSERT_TRUE(std::is_trivially_copyable<BitaxeStatus>::value);
    TEST_ASSERT_TRUE(std::is_standard_layout<BitaxeStatus>::value);
}

void test_status_fits_in_rtc_slow_memory(void)
{
    // A RTC slow memory tem 8 KB no total e é compartilhada.
    TEST_ASSERT_LESS_THAN(512, (int) sizeof(BitaxeStatus));
}

void setUp(void) {}
void tearDown(void) {}

// Testes no alvo usam setup()/loop(), não main(). O delay inicial dá tempo de
// a serial subir antes de os resultados saírem.
void setup()
{
    delay(2000);
    UNITY_BEGIN();
    RUN_TEST(test_status_is_pod_so_it_survives_deep_sleep);
    RUN_TEST(test_status_fits_in_rtc_slow_memory);
    UNITY_END();
}

void loop() {}
