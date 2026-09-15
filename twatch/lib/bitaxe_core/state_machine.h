#pragma once

#include <stdint.h>

// Teto de vida do portal, e por tabela do SoftAP. É constante de compilação de
// propósito: é uma trava de segurança de bateria, e não faria sentido o usuário
// poder afrouxá-la pelo próprio portal.
#define IDLE_TIMEOUT_CONFIG_MS 180000

enum AppState
{
    STATE_DEEP_SLEEP,
    STATE_RELOGIO,
    STATE_PAINEL,
    STATE_CONFIG,
};

enum AppEvent
{
    EVENT_WAKE,          // despertou do deep sleep (toque ou botão PEK)
    EVENT_TOUCH,         // toque com a tela já acesa
    EVENT_TOUCH_CONFIG,  // toque na faixa da engrenagem, no rodapé do relógio
    EVENT_TICK,          // passagem de tempo
    EVENT_TOUCH_NEXT,    // toque no painel que avanca para o proximo Bitaxe
};

// Os tempos de ociosidade do relógio e do painel deixaram de ser constantes:
// vêm das Settings, gravadas pelo portal.
struct IdleTimeouts
{
    uint32_t relogioMs;
    uint32_t painelMs;
};

// Pura: recebe estado, evento, tempo ocioso e os tetos, devolve o próximo estado.
AppState state_next(AppState current, AppEvent event, uint32_t idleMs, const IdleTimeouts &t);
