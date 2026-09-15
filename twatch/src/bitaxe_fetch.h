#pragma once

#include <stdint.h>

#include "bitaxe_status.h"

enum FetchResult
{
    FETCH_OK,
    FETCH_NO_NETWORK,   // WiFi não conectou
    FETCH_NO_RESPONSE,  // timeout ou erro de transporte
    FETCH_BAD_STATUS,   // HTTP != 200 (401 = falta o header Origin)
    FETCH_BAD_JSON,     // resposta ilegível; o cache é preservado
};

// Presume WiFi já conectado. Não mexe em `out` a menos que devolva FETCH_OK.
// `host` é o IPv4 do Bitaxe e `timeoutMs` o teto de conexão e de leitura, os
// dois vindos das Settings.
FetchResult bitaxe_fetch(const char *host, uint32_t timeoutMs, BitaxeStatus &out, int &httpCode);
