#include "bitaxe_fetch.h"

#include <HTTPClient.h>
#include <WiFi.h>

#include "bitaxe_parse.h"

FetchResult bitaxe_fetch(const char *host, uint32_t timeoutMs, BitaxeStatus &out, int &httpCode)
{
    httpCode = 0;

    if (WiFi.status() != WL_CONNECTED) {
        return FETCH_NO_NETWORK;
    }

    const String url = String("http://") + host + "/api/system/info";

    HTTPClient http;
    http.setConnectTimeout(timeoutMs);
    http.setTimeout(timeoutMs);

    if (!http.begin(url)) {
        return FETCH_NO_RESPONSE;
    }

    // O ESP-Miner valida private-network CORS: sem o header Origin a resposta
    // é 401, mesmo com IP correto e o dispositivo saudável.
    http.addHeader("Origin", String("http://") + host);

    httpCode = http.GET();
    if (httpCode != HTTP_CODE_OK) {
        http.end();
        return httpCode <= 0 ? FETCH_NO_RESPONSE : FETCH_BAD_STATUS;
    }

    const String body = http.getString();
    http.end();

    return bitaxe_parse(body.c_str(), out) ? FETCH_OK : FETCH_BAD_JSON;
}
