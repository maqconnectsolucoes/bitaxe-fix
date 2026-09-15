#include "state_machine.h"

AppState state_next(AppState current, AppEvent event, uint32_t idleMs, const IdleTimeouts &t)
{
    switch (current) {
    case STATE_DEEP_SLEEP:
        return event == EVENT_WAKE ? STATE_RELOGIO : STATE_DEEP_SLEEP;

    case STATE_RELOGIO:
        if (event == EVENT_TOUCH_CONFIG) {
            return STATE_CONFIG;
        }
        if (event == EVENT_TOUCH || event == EVENT_TOUCH_NEXT) {
            return STATE_PAINEL;
        }
        return idleMs >= t.relogioMs ? STATE_DEEP_SLEEP : STATE_RELOGIO;

    case STATE_PAINEL:
        // Alternar entre dispositivos mantem o painel e reinicia a ociosidade.
        // Quem decide entre alternar e sair e o main, via panel_cycle_next.
        if (event == EVENT_TOUCH_NEXT) {
            return STATE_PAINEL;
        }
        // O painel não tem engrenagem; tratar os dois toques igual mantém a
        // função total.
        if (event == EVENT_TOUCH || event == EVENT_TOUCH_CONFIG) {
            return STATE_RELOGIO;
        }
        return idleMs >= t.painelMs ? STATE_DEEP_SLEEP : STATE_PAINEL;

    case STATE_CONFIG:
        if (event == EVENT_TOUCH || event == EVENT_TOUCH_CONFIG || event == EVENT_TOUCH_NEXT) {
            return STATE_RELOGIO;
        }
        return idleMs >= IDLE_TIMEOUT_CONFIG_MS ? STATE_DEEP_SLEEP : STATE_CONFIG;
    }
    return STATE_DEEP_SLEEP;
}
