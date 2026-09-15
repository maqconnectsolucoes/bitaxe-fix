#include "ui_layout.h"

ClockHit clock_hit(int16_t x, int16_t y)
{
    // A faixa cobre a largura inteira: x não entra na decisão hoje, mas fica na
    // assinatura porque é o par natural do que o touch entrega, e um segundo
    // botão no rodapé só mexeria aqui.
    (void) x;
    return y >= CONFIG_STRIP_TOP_Y ? HIT_CONFIG : HIT_PAINEL;
}
