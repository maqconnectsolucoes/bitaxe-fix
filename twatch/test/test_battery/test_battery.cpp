#include <Arduino.h>
#include <unity.h>

#include "battery.h"

void test_percentual_do_axp_vence_a_tensao(void)
{
    // Com o registrador valido, a tensao nem e consultada.
    TEST_ASSERT_EQUAL(77, battery_percent(77, 3500));
}

void test_axp_zero_cai_na_tensao(void)
{
    // getBattPercentage devolve 0 tanto para "sem bateria" quanto para bit de
    // validade baixo. Zero nunca e tratado como carga real.
    TEST_ASSERT_EQUAL(50, battery_percent(0, 3750));
}

void test_axp_negativo_cai_na_tensao(void)
{
    TEST_ASSERT_EQUAL(50, battery_percent(-1, 3750));
}

void test_axp_acima_de_cem_cai_na_tensao(void)
{
    TEST_ASSERT_EQUAL(100, battery_percent(120, 4200));
}

void test_sem_axp_e_sem_tensao_e_desconhecido(void)
{
    TEST_ASSERT_EQUAL(BATTERY_UNKNOWN, battery_percent(0, 0));
}

void test_tensao_abaixo_do_piso_satura_em_zero(void)
{
    TEST_ASSERT_EQUAL(0, battery_percent(0, 3100));
}

void test_tensao_acima_do_teto_satura_em_cem(void)
{
    TEST_ASSERT_EQUAL(100, battery_percent(0, 4300));
}

void test_nivel_abaixo_do_minimo_e_baixo(void)
{
    TEST_ASSERT_EQUAL(BATTERY_LEVEL_LOW, battery_level(9, 15));
}

void test_nivel_no_minimo_ja_e_medio(void)
{
    // O corte do painel usa "< minimo"; a cor tem que concordar com ele.
    TEST_ASSERT_EQUAL(BATTERY_LEVEL_MID, battery_level(15, 15));
}

void test_nivel_em_cinquenta_ainda_e_medio(void)
{
    TEST_ASSERT_EQUAL(BATTERY_LEVEL_MID, battery_level(50, 15));
}

void test_nivel_acima_de_cinquenta_e_alto(void)
{
    TEST_ASSERT_EQUAL(BATTERY_LEVEL_HIGH, battery_level(51, 15));
}

void test_nivel_desconhecido_nao_vira_baixo(void)
{
    // Pintar de vermelho o que nao se sabe seria mentira com cara de alarme.
    TEST_ASSERT_EQUAL(BATTERY_LEVEL_UNKNOWN, battery_level(BATTERY_UNKNOWN, 15));
}

void test_segmentos_zero_por_cento(void)
{
    TEST_ASSERT_EQUAL(0, battery_segments(0));
}

void test_segmentos_um_por_cento_acende_um(void)
{
    TEST_ASSERT_EQUAL(1, battery_segments(1));
}

void test_segmentos_cem_por_cento_acende_todos(void)
{
    TEST_ASSERT_EQUAL(BATTERY_SEGMENTS, battery_segments(100));
}

void test_segmentos_desconhecido_nao_acende_nada(void)
{
    TEST_ASSERT_EQUAL(0, battery_segments(BATTERY_UNKNOWN));
}

void setUp(void) {}
void tearDown(void) {}

// Testes no alvo usam setup()/loop(), nao main().
void setup()
{
    delay(2000);
    UNITY_BEGIN();
    RUN_TEST(test_percentual_do_axp_vence_a_tensao);
    RUN_TEST(test_axp_zero_cai_na_tensao);
    RUN_TEST(test_axp_negativo_cai_na_tensao);
    RUN_TEST(test_axp_acima_de_cem_cai_na_tensao);
    RUN_TEST(test_sem_axp_e_sem_tensao_e_desconhecido);
    RUN_TEST(test_tensao_abaixo_do_piso_satura_em_zero);
    RUN_TEST(test_tensao_acima_do_teto_satura_em_cem);
    RUN_TEST(test_nivel_abaixo_do_minimo_e_baixo);
    RUN_TEST(test_nivel_no_minimo_ja_e_medio);
    RUN_TEST(test_nivel_em_cinquenta_ainda_e_medio);
    RUN_TEST(test_nivel_acima_de_cinquenta_e_alto);
    RUN_TEST(test_nivel_desconhecido_nao_vira_baixo);
    RUN_TEST(test_segmentos_zero_por_cento);
    RUN_TEST(test_segmentos_um_por_cento_acende_um);
    RUN_TEST(test_segmentos_cem_por_cento_acende_todos);
    RUN_TEST(test_segmentos_desconhecido_nao_acende_nada);
    UNITY_END();
}

void loop() {}
