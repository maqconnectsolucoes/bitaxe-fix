#include "bitaxe_parse.h"

#include <ArduinoJson.h>
#include <string.h>

static void copyField(char *dest, size_t cap, const char *src)
{
    if (!src) {
        dest[0] = '\0';
        return;
    }
    strncpy(dest, src, cap - 1);
    dest[cap - 1] = '\0';
}

bool bitaxe_parse(const char *json, BitaxeStatus &out)
{
    if (!json || json[0] == '\0') {
        return false;
    }

    JsonDocument doc;
    if (deserializeJson(doc, json)) {
        return false;  // preserva o cache do chamador
    }

    // A spec manda descartar resposta incompleta: sobrescrever o cache com
    // zeros faria o painel mostrar um minerador parado que não está parado.
    if (!doc["hashRate_10m"].is<float>() || !doc["power"].is<float>()) {
        return false;
    }

    BitaxeStatus s = {};
    s.hashRate10m = doc["hashRate_10m"] | 0.0f;
    s.power = doc["power"] | 0.0f;
    s.frequency = doc["frequency"] | 0.0f;
    s.coreVoltageActual = doc["coreVoltageActual"] | 0.0f;
    s.sharesAccepted = doc["sharesAccepted"] | 0u;
    copyField(s.bestDiff, BITAXE_STR_LEN, doc["bestDiff"] | (const char *) nullptr);
    copyField(s.hostname, BITAXE_STR_LEN, doc["hostname"] | (const char *) nullptr);

    out = s;
    return true;
}
