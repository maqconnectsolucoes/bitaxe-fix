#include "screen.h"

#include "ui_layout.h"

static TTGOClass *g_watch = nullptr;
static TFT_eSprite *g_sprite = nullptr;
static uint8_t g_brightness = 100;

bool screen_begin(TTGOClass *watch)
{
    g_watch = watch;
    if (g_sprite != nullptr) {
        return true;
    }

    g_sprite = new TFT_eSprite(watch->tft);
    g_sprite->setColorDepth(16);

    // 240*240*2 = 115200 bytes. createSprite cai em ps_calloc sozinho quando ha
    // PSRAM (TFT_eSPI.cpp:5506), entao isso nao disputa com a heap interna.
    if (g_sprite->createSprite(SCREEN_W, SCREEN_H) == nullptr) {
        delete g_sprite;
        g_sprite = nullptr;
        return false;
    }
    return true;
}

TFT_eSPI &screen_canvas(void)
{
    if (g_sprite != nullptr) {
        return *g_sprite;
    }
    return *g_watch->tft;
}

void screen_flush(void)
{
    if (g_sprite != nullptr) {
        g_sprite->pushSprite(0, 0);
    }
}

void screen_brightness(uint8_t percent, bool fade)
{
    if (percent > 100) {
        percent = 100;
    }
    const int target = (percent * 255) / 100;
    const int from = (g_brightness * 255) / 100;

    if (!fade || from == target) {
        g_watch->setBrightness((uint8_t) target);
    } else {
        // 16 degraus de 8 ms: 128 ms no total, curto o bastante para nao
        // atrasar a resposta ao toque.
        for (int i = 1; i <= 16; i++) {
            g_watch->setBrightness((uint8_t) (from + (target - from) * i / 16));
            delay(8);
        }
    }
    g_brightness = percent;
}
