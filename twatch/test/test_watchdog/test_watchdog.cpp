#include <Arduino.h>
#include <unity.h>

#include "battery.h"
#include "watchdog.h"

// Dois aparelhos saudaveis, minerando. Cada teste estraga UM aspecto.
static WatchdogInputs saudavel(void)
{
    WatchdogInputs in = {};
    in.deviceCount = 2;
    in.online[0] = true;
    in.online[1] = true;
    in.hashRate[0] = 1362.0f;
    in.hashRate[1] = 1288.0f;
    return in;
}

void test_tudo_bem_nao_alerta(void)
{
    TEST_ASSERT_FALSE(watchdog_should_alert(saudavel()));
}

void test_dispositivo_offline_alerta(void)
{
    WatchdogInputs in = saudavel();
    in.online[1] = false;
    TEST_ASSERT_TRUE(watchdog_should_alert(in));
}

void test_hashrate_zerado_alerta(void)
{
    // Responde, mas parou de minerar: e falha tanto quanto nao responder.
    WatchdogInputs in = saudavel();
    in.hashRate[0] = 0.0f;
    TEST_ASSERT_TRUE(watchdog_should_alert(in));
}

void test_segundo_dispositivo_nao_configurado_e_ignorado(void)
{
    // Com deviceCount 1, o lixo do slot 1 nao pode disparar alerta.
    WatchdogInputs in = saudavel();
    in.deviceCount = 1;
    in.online[1] = false;
    in.hashRate[1] = 0.0f;
    TEST_ASSERT_FALSE(watchdog_should_alert(in));
}

void test_contagem_invalida_nao_alerta(void)
{
    WatchdogInputs in = saudavel();
    in.deviceCount = 0;
    TEST_ASSERT_FALSE(watchdog_should_alert(in));
}

void test_contagem_acima_do_teto_nao_alerta(void)
{
    // Impossivel hoje (nenhuma tela configura um terceiro Bitaxe), mas no dia
    // em que alguem configurar, este guard e a unica coisa entre a entrada
    // invalida e um acesso fora dos arrays online[]/hashRate[].
    WatchdogInputs in = saudavel();
    in.deviceCount = 3;
    TEST_ASSERT_FALSE(watchdog_should_alert(in));
}

void test_intervalo_zero_nao_roda(void)
{
    // Zero e o padrao de fabrica: a vigia so existe se o usuario pedir.
    TEST_ASSERT_FALSE(watchdog_should_run(0, 90, 15));
}

void test_bateria_boa_roda(void)
{
    TEST_ASSERT_TRUE(watchdog_should_run(15, 90, 15));
}

void test_bateria_abaixo_do_minimo_nao_roda(void)
{
    TEST_ASSERT_FALSE(watchdog_should_run(15, 9, 15));
}

void test_bateria_exatamente_no_minimo_roda(void)
{
    TEST_ASSERT_TRUE(watchdog_should_run(15, 15, 15));
}

void test_bateria_desconhecida_nao_bloqueia(void)
{
    // Mesma regra que o painel ja aplica: so bloqueia o que se sabe estar baixo.
    TEST_ASSERT_TRUE(watchdog_should_run(15, BATTERY_UNKNOWN, 15));
}

void setUp(void) {}
void tearDown(void) {}

void setup()
{
    delay(2000);
    UNITY_BEGIN();
    RUN_TEST(test_tudo_bem_nao_alerta);
    RUN_TEST(test_dispositivo_offline_alerta);
    RUN_TEST(test_hashrate_zerado_alerta);
    RUN_TEST(test_segundo_dispositivo_nao_configurado_e_ignorado);
    RUN_TEST(test_contagem_invalida_nao_alerta);
    RUN_TEST(test_contagem_acima_do_teto_nao_alerta);
    RUN_TEST(test_intervalo_zero_nao_roda);
    RUN_TEST(test_bateria_boa_roda);
    RUN_TEST(test_bateria_abaixo_do_minimo_nao_roda);
    RUN_TEST(test_bateria_exatamente_no_minimo_roda);
    RUN_TEST(test_bateria_desconhecida_nao_bloqueia);
    UNITY_END();
}

void loop() {}
