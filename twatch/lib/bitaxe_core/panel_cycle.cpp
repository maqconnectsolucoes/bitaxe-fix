#include "panel_cycle.h"

int panel_cycle_next(int current, int deviceCount)
{
    if (deviceCount < 1 || deviceCount > PANEL_CYCLE_MAX_DEVICES) {
        return PANEL_CYCLE_CLOCK;
    }
    if (current == PANEL_CYCLE_CLOCK) {
        return 0;
    }
    // Indice invalido inclui o caso real de o portal ter desligado o segundo
    // host enquanto o painel estava nele.
    if (current < 0 || current >= deviceCount) {
        return PANEL_CYCLE_CLOCK;
    }

    const int next = current + 1;
    return next >= deviceCount ? PANEL_CYCLE_CLOCK : next;
}
