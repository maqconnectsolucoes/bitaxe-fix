#include <Arduino.h>
#include <string.h>
#include <unity.h>

#include "settings.h"

// Configuração plausível e válida; cada teste estraga UM campo a partir dela.
static Settings baseline(void)
{
    Settings s = {};
    strcpy(s.wifiSsid, "minha-rede");
    strcpy(s.wifiPass, "segredo123");
    strcpy(s.bitaxeHost, "192.168.1.100");
    s.tzMinutes = -180;
    s.wifiTimeoutMs = 8000;
    s.httpTimeoutMs = 4000;
    s.batteryMinPct = 15;
    s.idleRelogioMs = 5000;
    s.idlePainelMs = 15000;
    strcpy(s.bitaxeHost2, "");
    s.brightnessClock = 40;
    s.brightnessPanel = 100;
    s.watchIntervalMin = 0;
    return s;
}

static SettingsForm baselineForm(void)
{
    SettingsForm f;
    f.ssid = "outra-rede";
    f.pass = "";
    f.host = "10.0.0.7";
    f.tzMinutes = "-180";
    f.wifiTimeoutMs = "8000";
    f.httpTimeoutMs = "4000";
    f.batteryMinPct = "15";
    f.idleRelogioMs = "5000";
    f.idlePainelMs = "15000";
    f.host2 = "";
    f.brightnessClock = "40";
    f.brightnessPanel = "100";
    f.watchIntervalMin = "0";
    return f;
}

void test_baseline_e_valido(void)
{
    TEST_ASSERT_EQUAL(SETTINGS_OK, settings_validate(baseline()));
}

void test_ssid_vazio_reprova(void)
{
    Settings s = baseline();
    s.wifiSsid[0] = '\0';
    TEST_ASSERT_EQUAL(SETTINGS_ERR_SSID, settings_validate(s));
}

void test_senha_de_sete_caracteres_reprova(void)
{
    // WPA2 não aceita 1..7; deixar passar viraria um AP que nunca conecta.
    Settings s = baseline();
    strcpy(s.wifiPass, "1234567");
    TEST_ASSERT_EQUAL(SETTINGS_ERR_PASS, settings_validate(s));
}

void test_senha_vazia_aprova_rede_aberta(void)
{
    Settings s = baseline();
    s.wifiPass[0] = '\0';
    TEST_ASSERT_EQUAL(SETTINGS_OK, settings_validate(s));
}

void test_host_com_nome_reprova(void)
{
    // bitaxe.local resolve por IPv6 e a API devolve 401. Aceitar hostname aqui
    // seria plantar esse bug de propósito.
    Settings s = baseline();
    strcpy(s.bitaxeHost, "bitaxe.local");
    TEST_ASSERT_EQUAL(SETTINGS_ERR_HOST, settings_validate(s));
}

void test_host_com_octeto_acima_de_255_reprova(void)
{
    Settings s = baseline();
    strcpy(s.bitaxeHost, "192.168.1.256");
    TEST_ASSERT_EQUAL(SETTINGS_ERR_HOST, settings_validate(s));
}

void test_host_incompleto_reprova(void)
{
    Settings s = baseline();
    strcpy(s.bitaxeHost, "192.168.1");
    TEST_ASSERT_EQUAL(SETTINGS_ERR_HOST, settings_validate(s));
}

void test_fuso_fora_da_faixa_reprova(void)
{
    Settings s = baseline();
    s.tzMinutes = 900;
    TEST_ASSERT_EQUAL(SETTINGS_ERR_TZ, settings_validate(s));
}

void test_fuso_fora_do_passo_reprova(void)
{
    Settings s = baseline();
    s.tzMinutes = -190;
    TEST_ASSERT_EQUAL(SETTINGS_ERR_TZ, settings_validate(s));
}

void test_bateria_acima_de_cinquenta_reprova(void)
{
    // Acima disso o painel nunca abriria.
    Settings s = baseline();
    s.batteryMinPct = 51;
    TEST_ASSERT_EQUAL(SETTINGS_ERR_BATTERY, settings_validate(s));
}

void test_ociosidade_do_painel_acima_do_teto_reprova(void)
{
    Settings s = baseline();
    s.idlePainelMs = 130000;
    TEST_ASSERT_EQUAL(SETTINGS_ERR_IDLE_PAINEL, settings_validate(s));
}

void test_form_senha_vazia_preserva_a_gravada(void)
{
    Settings out = {};
    TEST_ASSERT_EQUAL(SETTINGS_OK, settings_apply_form(baseline(), baselineForm(), out));
    TEST_ASSERT_EQUAL_STRING("segredo123", out.wifiPass);
    TEST_ASSERT_EQUAL_STRING("outra-rede", out.wifiSsid);
    TEST_ASSERT_EQUAL_STRING("10.0.0.7", out.bitaxeHost);
}

void test_form_senha_preenchida_substitui(void)
{
    SettingsForm f = baselineForm();
    f.pass = "novasenha";
    Settings out = {};
    TEST_ASSERT_EQUAL(SETTINGS_OK, settings_apply_form(baseline(), f, out));
    TEST_ASSERT_EQUAL_STRING("novasenha", out.wifiPass);
}

void test_form_numero_com_lixo_reprova(void)
{
    // strtol sozinho leria "4000abc" como 4000. O parser precisa ser estrito.
    SettingsForm f = baselineForm();
    f.httpTimeoutMs = "4000abc";
    Settings out = {};
    TEST_ASSERT_EQUAL(SETTINGS_ERR_HTTP_TIMEOUT, settings_apply_form(baseline(), f, out));
}

void test_form_numero_vazio_reprova(void)
{
    SettingsForm f = baselineForm();
    f.wifiTimeoutMs = "";
    Settings out = {};
    TEST_ASSERT_EQUAL(SETTINGS_ERR_WIFI_TIMEOUT, settings_apply_form(baseline(), f, out));
}

void test_form_ssid_longo_demais_reprova(void)
{
    // 33 caracteres: recusa em vez de truncar em silêncio.
    SettingsForm f = baselineForm();
    f.ssid = "123456789012345678901234567890123";
    Settings out = {};
    TEST_ASSERT_EQUAL(SETTINGS_ERR_SSID, settings_apply_form(baseline(), f, out));
}

void test_form_fuso_fora_do_passo_reprova(void)
{
    SettingsForm f = baselineForm();
    f.tzMinutes = "-190";
    Settings out = {};
    TEST_ASSERT_EQUAL(SETTINGS_ERR_TZ, settings_apply_form(baseline(), f, out));
}

void test_equal_detecta_diferenca_em_cada_campo(void)
{
    Settings a = baseline();
    TEST_ASSERT_TRUE(settings_equal(a, baseline()));

    Settings b = baseline();
    strcpy(b.bitaxeHost, "10.0.0.1");
    TEST_ASSERT_FALSE(settings_equal(a, b));

    Settings c = baseline();
    c.tzMinutes = 0;
    TEST_ASSERT_FALSE(settings_equal(a, c));

    Settings d = baseline();
    d.idlePainelMs = 20000;
    TEST_ASSERT_FALSE(settings_equal(a, d));
}

void test_host2_vazio_aprova(void)
{
    // Vazio e o caso normal: um dispositivo so.
    Settings s = baseline();
    TEST_ASSERT_EQUAL(SETTINGS_OK, settings_validate(s));
}

void test_host2_valido_aprova(void)
{
    Settings s = baseline();
    strcpy(s.bitaxeHost2, "10.1.1.126");
    TEST_ASSERT_EQUAL(SETTINGS_OK, settings_validate(s));
}

void test_host2_igual_ao_primeiro_reprova(void)
{
    // Erro de digitacao que produziria duas telas identicas sem sintoma nenhum.
    Settings s = baseline();
    strcpy(s.bitaxeHost2, s.bitaxeHost);
    TEST_ASSERT_EQUAL(SETTINGS_ERR_HOST2, settings_validate(s));
}

void test_host2_com_nome_reprova(void)
{
    Settings s = baseline();
    strcpy(s.bitaxeHost2, "bitaxe.local");
    TEST_ASSERT_EQUAL(SETTINGS_ERR_HOST2, settings_validate(s));
}

void test_contagem_de_dispositivos_sem_segundo_host(void)
{
    TEST_ASSERT_EQUAL(1, settings_device_count(baseline()));
}

void test_contagem_de_dispositivos_com_segundo_host(void)
{
    Settings s = baseline();
    strcpy(s.bitaxeHost2, "10.1.1.126");
    TEST_ASSERT_EQUAL(2, settings_device_count(s));
}

void test_form_host2_vazio_desliga_o_segundo(void)
{
    // Diferente da senha: aqui vazio significa "desligar", nao "manter".
    Settings current = baseline();
    strcpy(current.bitaxeHost2, "10.1.1.126");

    SettingsForm f = baselineForm();
    f.host2 = "";

    Settings out = {};
    TEST_ASSERT_EQUAL(SETTINGS_OK, settings_apply_form(current, f, out));
    TEST_ASSERT_EQUAL_STRING("", out.bitaxeHost2);
}

void test_form_host2_nulo_desliga_o_segundo(void)
{
    Settings current = baseline();
    strcpy(current.bitaxeHost2, "10.1.1.126");

    SettingsForm f = baselineForm();
    f.host2 = NULL;

    Settings out = {};
    TEST_ASSERT_EQUAL(SETTINGS_OK, settings_apply_form(current, f, out));
    TEST_ASSERT_EQUAL_STRING("", out.bitaxeHost2);
}

void test_form_host2_igual_ao_primeiro_reprova(void)
{
    SettingsForm f = baselineForm();
    f.host = "10.0.0.7";
    f.host2 = "10.0.0.7";

    Settings out = {};
    TEST_ASSERT_EQUAL(SETTINGS_ERR_HOST2, settings_apply_form(baseline(), f, out));
}

void test_equal_detecta_diferenca_no_host2(void)
{
    Settings a = baseline();
    Settings b = baseline();
    strcpy(b.bitaxeHost2, "10.1.1.126");
    TEST_ASSERT_FALSE(settings_equal(a, b));
}

void test_brilho_abaixo_do_minimo_reprova(void)
{
    // Abaixo de 10% a tela fica ilegivel no sol — e o usuario nao teria como
    // voltar atras sem enxergar o portal.
    Settings s = baseline();
    s.brightnessClock = 5;
    TEST_ASSERT_EQUAL(SETTINGS_ERR_BRIGHTNESS, settings_validate(s));
}

void test_brilho_do_painel_acima_de_cem_reprova(void)
{
    Settings s = baseline();
    s.brightnessPanel = 120;
    TEST_ASSERT_EQUAL(SETTINGS_ERR_BRIGHTNESS, settings_validate(s));
}

void test_vigia_desligada_aprova(void)
{
    Settings s = baseline();
    s.watchIntervalMin = 0;
    TEST_ASSERT_EQUAL(SETTINGS_OK, settings_validate(s));
}

void test_vigia_abaixo_do_minimo_reprova(void)
{
    // Menos de 5 min drenaria a bateria em uma tarde.
    Settings s = baseline();
    s.watchIntervalMin = 3;
    TEST_ASSERT_EQUAL(SETTINGS_ERR_WATCH, settings_validate(s));
}

void test_vigia_no_minimo_aprova(void)
{
    Settings s = baseline();
    s.watchIntervalMin = SETTINGS_WATCH_MIN_MINUTES;
    TEST_ASSERT_EQUAL(SETTINGS_OK, settings_validate(s));
}

void test_vigia_acima_do_teto_reprova(void)
{
    Settings s = baseline();
    s.watchIntervalMin = SETTINGS_WATCH_MAX_MINUTES + 1;
    TEST_ASSERT_EQUAL(SETTINGS_ERR_WATCH, settings_validate(s));
}

void test_form_vigia_com_lixo_reprova(void)
{
    SettingsForm f = baselineForm();
    f.watchIntervalMin = "15min";

    Settings out = {};
    TEST_ASSERT_EQUAL(SETTINGS_ERR_WATCH, settings_apply_form(baseline(), f, out));
}

void test_form_brilho_valido_grava(void)
{
    SettingsForm f = baselineForm();
    f.brightnessClock = "25";

    Settings out = {};
    TEST_ASSERT_EQUAL(SETTINGS_OK, settings_apply_form(baseline(), f, out));
    TEST_ASSERT_EQUAL(25, out.brightnessClock);
}

void setUp(void) {}
void tearDown(void) {}

// Testes no alvo usam setup()/loop(), não main().
void setup()
{
    delay(2000);
    UNITY_BEGIN();
    RUN_TEST(test_baseline_e_valido);
    RUN_TEST(test_ssid_vazio_reprova);
    RUN_TEST(test_senha_de_sete_caracteres_reprova);
    RUN_TEST(test_senha_vazia_aprova_rede_aberta);
    RUN_TEST(test_host_com_nome_reprova);
    RUN_TEST(test_host_com_octeto_acima_de_255_reprova);
    RUN_TEST(test_host_incompleto_reprova);
    RUN_TEST(test_host2_vazio_aprova);
    RUN_TEST(test_host2_valido_aprova);
    RUN_TEST(test_host2_igual_ao_primeiro_reprova);
    RUN_TEST(test_host2_com_nome_reprova);
    RUN_TEST(test_contagem_de_dispositivos_sem_segundo_host);
    RUN_TEST(test_contagem_de_dispositivos_com_segundo_host);
    RUN_TEST(test_form_host2_vazio_desliga_o_segundo);
    RUN_TEST(test_form_host2_nulo_desliga_o_segundo);
    RUN_TEST(test_form_host2_igual_ao_primeiro_reprova);
    RUN_TEST(test_equal_detecta_diferenca_no_host2);
    RUN_TEST(test_fuso_fora_da_faixa_reprova);
    RUN_TEST(test_fuso_fora_do_passo_reprova);
    RUN_TEST(test_bateria_acima_de_cinquenta_reprova);
    RUN_TEST(test_ociosidade_do_painel_acima_do_teto_reprova);
    RUN_TEST(test_form_senha_vazia_preserva_a_gravada);
    RUN_TEST(test_form_senha_preenchida_substitui);
    RUN_TEST(test_form_numero_com_lixo_reprova);
    RUN_TEST(test_form_numero_vazio_reprova);
    RUN_TEST(test_form_ssid_longo_demais_reprova);
    RUN_TEST(test_form_fuso_fora_do_passo_reprova);
    RUN_TEST(test_equal_detecta_diferenca_em_cada_campo);
    RUN_TEST(test_brilho_abaixo_do_minimo_reprova);
    RUN_TEST(test_brilho_do_painel_acima_de_cem_reprova);
    RUN_TEST(test_vigia_desligada_aprova);
    RUN_TEST(test_vigia_abaixo_do_minimo_reprova);
    RUN_TEST(test_vigia_no_minimo_aprova);
    RUN_TEST(test_vigia_acima_do_teto_reprova);
    RUN_TEST(test_form_vigia_com_lixo_reprova);
    RUN_TEST(test_form_brilho_valido_grava);
    UNITY_END();
}

void loop() {}
