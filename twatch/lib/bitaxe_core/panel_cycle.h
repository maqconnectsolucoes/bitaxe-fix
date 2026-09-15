#pragma once

// Valor de "nao esta no painel": tanto a entrada (vindo do relogio) quanto a
// saida (ciclo terminou) usam esta sentinela.
#define PANEL_CYCLE_CLOCK (-1)

// Teto de dispositivos. Casado com SNAPSHOT_SLOTS: passar disso significaria
// gravar cache em slot que nao existe.
#define PANEL_CYCLE_MAX_DEVICES 2

// Avanca o ciclo relogio -> dev 0 -> dev 1 -> relogio. Entrada invalida sai
// pelo relogio, que e sempre o estado seguro.
int panel_cycle_next(int current, int deviceCount);
