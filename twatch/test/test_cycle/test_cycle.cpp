#include <Arduino.h>
#include <unity.h>

#include "panel_cycle.h"

void test_do_relogio_entra_no_primeiro(void)
{
    TEST_ASSERT_EQUAL(0, panel_cycle_next(PANEL_CYCLE_CLOCK, 2));
}

void test_do_primeiro_vai_para_o_segundo(void)
{
    TEST_ASSERT_EQUAL(1, panel_cycle_next(0, 2));
}

void test_do_segundo_volta_para_o_relogio(void)
{
    TEST_ASSERT_EQUAL(PANEL_CYCLE_CLOCK, panel_cycle_next(1, 2));
}

void test_com_um_dispositivo_o_ciclo_tem_dois_passos(void)
{
    // Comportamento identico ao de antes do segundo Bitaxe existir.
    TEST_ASSERT_EQUAL(0, panel_cycle_next(PANEL_CYCLE_CLOCK, 1));
    TEST_ASSERT_EQUAL(PANEL_CYCLE_CLOCK, panel_cycle_next(0, 1));
}

void test_contagem_zero_sai_pelo_relogio(void)
{
    TEST_ASSERT_EQUAL(PANEL_CYCLE_CLOCK, panel_cycle_next(PANEL_CYCLE_CLOCK, 0));
}

void test_contagem_acima_do_teto_sai_pelo_relogio(void)
{
    TEST_ASSERT_EQUAL(PANEL_CYCLE_CLOCK, panel_cycle_next(0, PANEL_CYCLE_MAX_DEVICES + 1));
}

void test_indice_alem_da_contagem_sai_pelo_relogio(void)
{
    // Acontece de verdade: o portal desliga o segundo host enquanto o painel
    // esta no dispositivo 1.
    TEST_ASSERT_EQUAL(PANEL_CYCLE_CLOCK, panel_cycle_next(1, 1));
}

void test_indice_negativo_invalido_sai_pelo_relogio(void)
{
    TEST_ASSERT_EQUAL(PANEL_CYCLE_CLOCK, panel_cycle_next(-7, 2));
}

void setUp(void) {}
void tearDown(void) {}

void setup()
{
    delay(2000);
    UNITY_BEGIN();
    RUN_TEST(test_do_relogio_entra_no_primeiro);
    RUN_TEST(test_do_primeiro_vai_para_o_segundo);
    RUN_TEST(test_do_segundo_volta_para_o_relogio);
    RUN_TEST(test_com_um_dispositivo_o_ciclo_tem_dois_passos);
    RUN_TEST(test_contagem_zero_sai_pelo_relogio);
    RUN_TEST(test_contagem_acima_do_teto_sai_pelo_relogio);
    RUN_TEST(test_indice_alem_da_contagem_sai_pelo_relogio);
    RUN_TEST(test_indice_negativo_invalido_sai_pelo_relogio);
    UNITY_END();
}

void loop() {}
