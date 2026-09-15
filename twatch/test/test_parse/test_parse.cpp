#include <Arduino.h>
#include <unity.h>
#include <string.h>
#include "bitaxe_parse.h"

// Recorte de uma resposta real de /api/system/info.
static const char *VALID_JSON =
    "{\"hashRate\":1180.5,\"hashRate_10m\":1204.8,\"power\":17.2,"
    "\"frequency\":490,\"coreVoltageActual\":1150,\"sharesAccepted\":8421,"
    "\"bestDiff\":\"1.2T\",\"hostname\":\"bitaxe\",\"overheat_mode\":0}";

void test_parse_reads_expected_fields(void)
{
    BitaxeStatus s;
    TEST_ASSERT_TRUE(bitaxe_parse(VALID_JSON, s));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1204.8f, s.hashRate10m);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 17.2f, s.power);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 490.0f, s.frequency);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1150.0f, s.coreVoltageActual);
    TEST_ASSERT_EQUAL_UINT32(8421, s.sharesAccepted);
    TEST_ASSERT_EQUAL_STRING("1.2T", s.bestDiff);
    TEST_ASSERT_EQUAL_STRING("bitaxe", s.hostname);
}

void test_parse_rejects_truncated_json(void)
{
    BitaxeStatus s;
    TEST_ASSERT_FALSE(bitaxe_parse("{\"hashRate_10m\":120", s));
}

void test_parse_rejects_empty_input(void)
{
    BitaxeStatus s;
    TEST_ASSERT_FALSE(bitaxe_parse("", s));
}

void test_optional_fields_absent_still_parses(void)
{
    BitaxeStatus s;
    TEST_ASSERT_TRUE(bitaxe_parse("{\"hashRate_10m\":900.0,\"power\":15.5}", s));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 900.0f, s.hashRate10m);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 15.5f, s.power);
    TEST_ASSERT_EQUAL_UINT32(0, s.sharesAccepted);
    TEST_ASSERT_EQUAL_STRING("", s.bestDiff);
}

void test_missing_essential_field_is_rejected(void)
{
    BitaxeStatus s;
    TEST_ASSERT_FALSE(bitaxe_parse("{\"power\":17.2}", s));          // sem hashRate_10m
    TEST_ASSERT_FALSE(bitaxe_parse("{\"hashRate_10m\":900.0}", s));  // sem power
    TEST_ASSERT_FALSE(bitaxe_parse("{}", s));
}

void test_rejection_leaves_caller_struct_untouched(void)
{
    // O chamador guarda um cache válido; uma resposta ruim não pode corrompê-lo.
    BitaxeStatus s = {};
    s.hashRate10m = 1204.8f;
    s.power = 17.2f;
    strncpy(s.bestDiff, "1.2T", BITAXE_STR_LEN - 1);

    TEST_ASSERT_FALSE(bitaxe_parse("{\"power\":17.2}", s));
    TEST_ASSERT_FALSE(bitaxe_parse("nao é json", s));

    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1204.8f, s.hashRate10m);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 17.2f, s.power);
    TEST_ASSERT_EQUAL_STRING("1.2T", s.bestDiff);
}

void test_long_strings_are_truncated_not_overflowed(void)
{
    BitaxeStatus s;
    const char *json =
        "{\"hashRate_10m\":900.0,\"power\":15.5,"
        "\"hostname\":\"um-nome-de-host-absurdamente-longo-que-nao-cabe\"}";
    TEST_ASSERT_TRUE(bitaxe_parse(json, s));
    TEST_ASSERT_EQUAL_INT(BITAXE_STR_LEN - 1, (int) strlen(s.hostname));
    TEST_ASSERT_EQUAL_STRING("um-nome-de-host-absurda", s.hostname);
}

void setUp(void) {}
void tearDown(void) {}

// Testes no alvo usam setup()/loop(), não main().
void setup()
{
    delay(2000);
    UNITY_BEGIN();
    RUN_TEST(test_parse_reads_expected_fields);
    RUN_TEST(test_parse_rejects_truncated_json);
    RUN_TEST(test_parse_rejects_empty_input);
    RUN_TEST(test_optional_fields_absent_still_parses);
    RUN_TEST(test_missing_essential_field_is_rejected);
    RUN_TEST(test_rejection_leaves_caller_struct_untouched);
    RUN_TEST(test_long_strings_are_truncated_not_overflowed);
    UNITY_END();
}

void loop() {}
