#include "watchdog.h"

#include "battery.h"
#include "metrics.h"
#include "panel_cycle.h"

// Quarta constante do projeto a dizer "no maximo dois Bitaxe". Ela e
// independente de proposito — watchdog.h nao deve depender da navegacao — mas
// divergir das outras faria o limite do laco e o tamanho do array discordarem
// sem ninguem acusar.
static_assert(WATCHDOG_MAX_DEVICES == PANEL_CYCLE_MAX_DEVICES, "teto de dispositivos divergiu do ciclo");

bool watchdog_should_alert(const WatchdogInputs &in)
{
    if (in.deviceCount < 1 || in.deviceCount > WATCHDOG_MAX_DEVICES) {
        return false;
    }

    for (uint8_t i = 0; i < in.deviceCount; i++) {
        if (!in.online[i]) {
            return true;
        }
        // Mesma constante que o painel usa para decidir que o J/TH nao
        // significa nada: abaixo dela o aparelho nao esta minerando.
        if (in.hashRate[i] < METRICS_MIN_HASHRATE_GHS) {
            return true;
        }
    }
    return false;
}

bool watchdog_should_run(uint32_t intervalMinutes, int batteryPercent, uint8_t minPct)
{
    if (intervalMinutes == 0) {
        return false;
    }
    // Desconhecida nao bloqueia: senao um AXP202 com registrador ruim
    // desligaria a vigia para sempre, sem sintoma.
    if (batteryPercent == BATTERY_UNKNOWN) {
        return true;
    }
    return batteryPercent >= (int) minPct;
}
