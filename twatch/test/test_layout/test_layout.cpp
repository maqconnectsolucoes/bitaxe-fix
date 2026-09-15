#include <Arduino.h>
#include <unity.h>

#include "ui_layout.h"

void test_toque_no_rodape_abre_config(void)
{
    TEST_ASSERT_EQUAL(HIT_CONFIG, clock_hit(120, 220));
}

void test_toque_no_meio_abre_painel(void)
{
    TEST_ASSERT_EQUAL(HIT_PAINEL, clock_hit(120, 100));
}

void test_fronteira_da_faixa(void)
{
    // 192 é a primeira linha da faixa; 191 é a última linha fora dela.
    TEST_ASSERT_EQUAL(HIT_CONFIG, clock_hit(120, CONFIG_STRIP_TOP_Y));
    TEST_ASSERT_EQUAL(HIT_PAINEL, clock_hit(120, CONFIG_STRIP_TOP_Y - 1));
}

void test_faixa_cobre_a_largura_inteira(void)
{
    TEST_ASSERT_EQUAL(HIT_CONFIG, clock_hit(0, 230));
    TEST_ASSERT_EQUAL(HIT_CONFIG, clock_hit(SCREEN_W - 1, 230));
}

void setUp(void) {}
void tearDown(void) {}

void setup()
{
    delay(2000);
    UNITY_BEGIN();
    RUN_TEST(test_toque_no_rodape_abre_config);
    RUN_TEST(test_toque_no_meio_abre_painel);
    RUN_TEST(test_fronteira_da_faixa);
    RUN_TEST(test_faixa_cobre_a_largura_inteira);
    UNITY_END();
}

void loop() {}
