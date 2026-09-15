#include <Arduino.h>
#include <unity.h>

#include "state_machine.h"

// Os valores que eram constantes de compilação passaram a vir das Settings.
// Os testes fixam os de fábrica para continuar verificando o mesmo contrato.
static const IdleTimeouts T = {5000, 15000};

void test_wake_from_sleep_goes_to_clock(void)
{
    TEST_ASSERT_EQUAL(STATE_RELOGIO, state_next(STATE_DEEP_SLEEP, EVENT_WAKE, 0, T));
}

void test_touch_on_clock_opens_panel(void)
{
    TEST_ASSERT_EQUAL(STATE_PAINEL, state_next(STATE_RELOGIO, EVENT_TOUCH, 0, T));
}

void test_touch_on_panel_returns_to_clock(void)
{
    TEST_ASSERT_EQUAL(STATE_RELOGIO, state_next(STATE_PAINEL, EVENT_TOUCH, 0, T));
}

void test_clock_sleeps_after_five_seconds_idle(void)
{
    TEST_ASSERT_EQUAL(STATE_RELOGIO, state_next(STATE_RELOGIO, EVENT_TICK, 4999, T));
    TEST_ASSERT_EQUAL(STATE_DEEP_SLEEP, state_next(STATE_RELOGIO, EVENT_TICK, 5000, T));
}

void test_panel_sleeps_after_fifteen_seconds_idle(void)
{
    TEST_ASSERT_EQUAL(STATE_PAINEL, state_next(STATE_PAINEL, EVENT_TICK, 14999, T));
    TEST_ASSERT_EQUAL(STATE_DEEP_SLEEP, state_next(STATE_PAINEL, EVENT_TICK, 15000, T));
}

void test_tick_never_wakes_from_sleep(void)
{
    // Só um evento externo acorda; o tempo passando, não.
    TEST_ASSERT_EQUAL(STATE_DEEP_SLEEP, state_next(STATE_DEEP_SLEEP, EVENT_TICK, 999999, T));
}

void test_timeouts_vem_do_parametro_nao_de_constante(void)
{
    // Prova que os tempos são mesmo configuráveis: com um teto maior, os
    // mesmos 5000 ms deixam de dormir.
    const IdleTimeouts longo = {30000, 60000};
    TEST_ASSERT_EQUAL(STATE_RELOGIO, state_next(STATE_RELOGIO, EVENT_TICK, 5000, longo));
}

void test_touch_no_rodape_abre_config(void)
{
    TEST_ASSERT_EQUAL(STATE_CONFIG, state_next(STATE_RELOGIO, EVENT_TOUCH_CONFIG, 0, T));
}

void test_touch_na_config_volta_ao_relogio(void)
{
    TEST_ASSERT_EQUAL(STATE_RELOGIO, state_next(STATE_CONFIG, EVENT_TOUCH, 0, T));
}

void test_config_dorme_apos_tres_minutos(void)
{
    TEST_ASSERT_EQUAL(STATE_CONFIG, state_next(STATE_CONFIG, EVENT_TICK, 179000, T));
    TEST_ASSERT_EQUAL(STATE_DEEP_SLEEP, state_next(STATE_CONFIG, EVENT_TICK, 180000, T));
}

void test_touch_config_no_painel_volta_ao_relogio(void)
{
    // O painel não tem engrenagem. Tratar os dois toques igual mantém a função
    // total, sem entrada que caia no default silencioso.
    TEST_ASSERT_EQUAL(STATE_RELOGIO, state_next(STATE_PAINEL, EVENT_TOUCH_CONFIG, 0, T));
}

void test_switch_keeps_panel_open(void)
{
    TEST_ASSERT_EQUAL(STATE_PAINEL, state_next(STATE_PAINEL, EVENT_TOUCH_NEXT, 0, T));
}

void test_plain_touch_on_panel_still_returns_to_clock(void)
{
    // E o fim do ciclo: quem decide qual evento emitir e o main, com
    // panel_cycle_next.
    TEST_ASSERT_EQUAL(STATE_RELOGIO, state_next(STATE_PAINEL, EVENT_TOUCH, 0, T));
}

void test_switch_on_clock_opens_panel(void)
{
    // Nao deveria acontecer, mas a funcao e total: nenhum evento pode deixar a
    // maquina num estado indefinido.
    TEST_ASSERT_EQUAL(STATE_PAINEL, state_next(STATE_RELOGIO, EVENT_TOUCH_NEXT, 0, T));
}

void test_switching_does_not_block_idle_sleep(void)
{
    TEST_ASSERT_EQUAL(STATE_DEEP_SLEEP, state_next(STATE_PAINEL, EVENT_TICK, 15000, T));
}

void setUp(void) {}
void tearDown(void) {}

// Testes no alvo usam setup()/loop(), não main().
void setup()
{
    delay(2000);
    UNITY_BEGIN();
    RUN_TEST(test_wake_from_sleep_goes_to_clock);
    RUN_TEST(test_touch_on_clock_opens_panel);
    RUN_TEST(test_touch_on_panel_returns_to_clock);
    RUN_TEST(test_clock_sleeps_after_five_seconds_idle);
    RUN_TEST(test_panel_sleeps_after_fifteen_seconds_idle);
    RUN_TEST(test_tick_never_wakes_from_sleep);
    RUN_TEST(test_timeouts_vem_do_parametro_nao_de_constante);
    RUN_TEST(test_touch_no_rodape_abre_config);
    RUN_TEST(test_touch_na_config_volta_ao_relogio);
    RUN_TEST(test_config_dorme_apos_tres_minutos);
    RUN_TEST(test_touch_config_no_painel_volta_ao_relogio);
    RUN_TEST(test_switch_keeps_panel_open);
    RUN_TEST(test_plain_touch_on_panel_still_returns_to_clock);
    RUN_TEST(test_switch_on_clock_opens_panel);
    RUN_TEST(test_switching_does_not_block_idle_sleep);
    UNITY_END();
}

void loop() {}
