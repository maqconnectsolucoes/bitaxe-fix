#pragma once

#include "settings.h"

// Mostrados na tela do relógio para o usuário digitar no celular.
extern const char *CONFIG_AP_SSID;
extern const char *CONFIG_AP_PASS;
extern const char *CONFIG_AP_URL;

// Sobe SoftAP, DNS cativo e servidor web. false se o AP não subir.
bool config_portal_begin(const Settings &current);

// Atende uma rodada de DNS e HTTP. Devolve true se alguma requisição foi
// servida — o chamador usa isso para rearmar o contador de ociosidade, senão o
// relógio dorme no meio da digitação no celular.
bool config_portal_poll();

// true depois de uma gravação bem-sucedida: o chamador deve reiniciar.
bool config_portal_should_restart();

// Derruba servidor, DNS e AP. Seguro chamar sem ter começado.
void config_portal_end();
