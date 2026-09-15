#pragma once

#include "settings.h"

// Lê do NVS. Campo ausente cai no valor de semente do config.h — é assim que
// "primeiro boot funciona sem passar pelo portal" sai de graça, campo a campo.
void settings_load(Settings &out);

// Grava e confere relendo. false significa que nada confiável foi gravado.
bool settings_save(const Settings &s);
