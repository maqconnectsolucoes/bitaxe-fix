#pragma once

#include <LilyGoWatch.h>

#include "bitaxe_status.h"

// `status` nulo significa cache vazio: desenha traços, não erro.
// `note` é um aviso curto no rodapé ("sem rede", "401") ou nullptr.
// `deviceIndex` e `deviceCount` so servem ao indicador do topo. Com um
// dispositivo o indicador nao e desenhado, e a tela fica identica a de antes.
void panel_view_draw(TTGOClass *watch, const BitaxeStatus *status, uint32_t ageSeconds,
                     const char *note, int deviceIndex, uint8_t deviceCount);
