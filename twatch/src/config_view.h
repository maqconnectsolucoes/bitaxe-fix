#pragma once

#include <LilyGoWatch.h>

// Tela cheia do modo de configuração.
void config_view_draw(TTGOClass *watch, const char *apSsid, const char *apPass,
                      const char *url, uint32_t secondsLeft, bool lowBattery);

// Redesenha a tela inteira (sprite: repintar tudo não pisca), uma vez por segundo.
void config_view_tick(TTGOClass *watch, uint32_t secondsLeft);

// Aviso de duas linhas ("salvo"/"reiniciando", falha ao subir o AP).
void config_view_message(TTGOClass *watch, const char *line1, const char *line2);
