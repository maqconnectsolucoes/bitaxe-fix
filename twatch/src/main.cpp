// Painel Bitaxe no TTGO T-Watch 2020 V1.
//
// Duas telas, WiFi só no painel, deep sleep entre consultas. O despertar
// reexecuta o setup(): só a RTC memory atravessa, e é por isso que o
// snapshot_store existe.

#include <Arduino.h>
#include <LilyGoWatch.h>
#include <esp_sleep.h>
#include <time.h>

#include "battery.h"
#include "bitaxe_fetch.h"
#include "clock_view.h"
#include "config.h"
#include "config_portal.h"
#include "config_view.h"
#include "net.h"
#include "panel_cycle.h"
#include "panel_view.h"
#include "screen.h"
#include "settings_store.h"
#include "snapshot_store.h"
#include "state_machine.h"
#include "ui_layout.h"
#include "watchdog.h"

// As tres constantes concordam por construcao (SNAPSHOT_SLOTS = quantos slots
// a RTC memory guarda, PANEL_CYCLE_MAX_DEVICES = teto do ciclo de toque,
// WATCHDOG_MAX_DEVICES = teto da vigia), mas nada no compilador obriga isso —
// esta e a unica unidade de traducao que enxerga os tres cabecalhos.
static_assert(SNAPSHOT_SLOTS == PANEL_CYCLE_MAX_DEVICES && SNAPSHOT_SLOTS == WATCHDOG_MAX_DEVICES,
              "tetos de dispositivos divergiram entre cache, ciclo e vigia");

static TTGOClass *watch = nullptr;
static AppState state = STATE_RELOGIO;
static uint32_t lastInteraction = 0;
static Settings g_settings;

static uint32_t lastCountdown = 0;

// Dispositivo em exibicao. PANEL_CYCLE_CLOCK enquanto o painel esta fechado.
static int g_panelDevice = PANEL_CYCLE_CLOCK;

// Nota de falha por dispositivo. Precisa sobreviver a alternancia: o painel
// alterna sem rede, e a nota do dispositivo 1 nao pode aparecer no 0.
static char g_note[SNAPSHOT_SLOTS][24];

// Marca quem respondeu na sessao atual do painel. Precisa sobreviver a
// alternancia pelo mesmo motivo que g_note: o redesenho nao tem rede, entao
// so este estado sabe que o dado e recente quando o RTC nao sabe.
static bool g_justFetched[SNAPSHOT_SLOTS];

// Zera o flag de dado recem-chegado nos dois slots. Chamado nos pontos em que
// o painel e abandonado (relogio, config, sono), sempre ao lado do reset de
// g_panelDevice: nenhuma sessao nova do painel pode herdar o justFetched de
// uma sessao anterior.
static void clearJustFetched()
{
    for (uint8_t i = 0; i < SNAPSHOT_SLOTS; i++) {
        g_justFetched[i] = false;
    }
}

// Os tetos de ociosidade vêm das Settings a cada chamada, não de uma cópia
// guardada: assim um valor recém-gravado passa a valer sem caminho extra.
static IdleTimeouts idleTimeouts()
{
    IdleTimeouts t;
    t.relogioMs = g_settings.idleRelogioMs;
    t.painelMs = g_settings.idlePainelMs;
    return t;
}

RTC_DATA_ATTR static bool g_timeIsValid = false;

// Sentinela para "idade desconhecida": nunca calculada a partir de um
// snapshot_age() real, só usada para forçar o painel a tratar o cache como
// velho quando não há como provar o contrário (ver cachedAge() abaixo).
static const uint32_t AGE_UNKNOWN = UINT32_MAX;

// Converte a leitura do PCF8563 em epoch Unix. Não assuma que RTC_Date expõe
// unixtime(): confirmado em pcf8563.h que os campos são
// year/month/day/hour/minute/second (sem essa conveniência).
static uint32_t nowEpoch()
{
    RTC_Date d = watch->rtc->getDateTime();
    struct tm t = {};
    t.tm_year = d.year - 1900;
    t.tm_mon = d.month - 1;
    t.tm_mday = d.day;
    t.tm_hour = d.hour;
    t.tm_min = d.minute;
    t.tm_sec = d.second;
    t.tm_isdst = -1;
    return (uint32_t) mktime(&t);
}

// snapshot_age() faz aritmética de epoch honesta, mas só faz sentido se o
// RTC já foi acertado por NTP alguma vez. Antes disso (primeiro uso, ou
// qualquer boot em que o painel nunca tenha sido aberto com rede disponível)
// o PCF8563 conta a partir de um valor arbitrário de fábrica, e comparar
// esse epoch com o gravado no snapshot pode até dar "nowEpoch <= gravado" —
// snapshot_age() devolve 0 nesse caso, o que o painel leria como "acabou de
// atualizar". Aqui isso é cortado na origem: sem hora válida, a idade é
// AGE_UNKNOWN, que o metrics_freshness() classifica como STALE (>= 600s) —
// nunca como frescor que não podemos provar.
static uint32_t cachedAge(int slot, bool hasCache)
{
    if (!hasCache) {
        return 0;
    }
    if (!g_timeIsValid) {
        return AGE_UNKNOWN;
    }
    return snapshot_age((uint8_t) slot, nowEpoch());
}

static const char *fetchNote(FetchResult r, int httpCode)
{
    static char buf[24];
    switch (r) {
    case FETCH_OK: return nullptr;
    case FETCH_NO_NETWORK: return "sem rede";
    case FETCH_NO_RESPONSE: return "sem resposta";
    case FETCH_BAD_JSON: return "resposta invalida";
    case FETCH_BAD_STATUS:
        snprintf(buf, sizeof(buf), "HTTP %d", httpCode);
        return buf;
    }
    return nullptr;
}

// Leitura unica da bateria. A leitura crua do AXP202 nao vale sozinha: o
// registrador devolve 0 tanto para "sem bateria" quanto para bit de validade
// baixo, e negativo quando o chip nao inicializou. Concentrada aqui para que
// o corte do painel, o aviso da config e a pilha do relogio nunca discordem.
static int batteryNow()
{
    const float millivolts = watch->power->getBattVoltage();
    return battery_percent(watch->power->getBattPercentage(),
                           millivolts > 0.0f ? (uint16_t) millivolts : 0);
}

// Alerta tatil. Silencio significa "esta tudo bem", entao isto so e chamado
// quando o watchdog aponta problema.
static void alertBuzz(uint8_t times)
{
    for (uint8_t i = 0; i < times; i++) {
        if (i > 0) {
            // Maior que os 200 ms que Motor::onec() mantem o motor ligado, senao
            // o pulso seguinte re-arma o Ticker antes de o anterior terminar e as
            // duas vibracoes viram uma so, longa.
            delay(300);
        }
        watch->shake();
    }
}

// Monta o modelo do relogio a partir da leitura unica de bateria acima.
static ClockViewModel clockModel()
{
    ClockViewModel vm;
    vm.timeIsValid = g_timeIsValid;
    vm.batteryPercent = batteryNow();
    vm.charging = watch->power->isChargeing();
    vm.batteryMinPct = g_settings.batteryMinPct;
    vm.steps = watch->bma->getCounter();
    return vm;
}

// Desenha um dispositivo a partir do cache. Sem rede: e o caminho da
// alternancia, e tambem o primeiro quadro ao abrir o painel.
//
// g_justFetched[index] marca o dado que acabou de chegar da rede nesta sessao
// do painel. Ele nao depende do RTC para ser atual, entao nao passa por
// cachedAge() — que sem NTP devolveria AGE_UNKNOWN e pintaria de cinza um dado
// de um segundo atras. E estado de arquivo, e nao parametro, precisamente
// para sobreviver ao redesenho da alternancia (main.cpp, ramo
// `previous == STATE_PAINEL`), que nao tem rede para recalcular nada.
static void drawDevice(int index)
{
    BitaxeStatus cached;
    const bool hasCache = snapshot_load((uint8_t) index, cached);

    const bool justFetched = g_justFetched[index];
    const char *note = g_note[index][0] != '\0' ? g_note[index] : nullptr;
    if (!justFetched && note == nullptr && hasCache && !g_timeIsValid) {
        note = "hora incerta";
    }

    panel_view_draw(watch, hasCache ? &cached : nullptr,
                    justFetched ? 0 : cachedAge(index, hasCache), note, index,
                    settings_device_count(g_settings));
}

// Consulta todos os dispositivos configurados numa conexao JA ABERTA e
// atualiza cache, notas e o retrato de saude. Nao conecta, nao desconecta e
// nao desenha: quem chama e que manda no radio e na tela. Existe uma copia so
// desta funcao porque a vigia periodica corre exatamente o mesmo laco.
static void pollDevices(WatchdogInputs &health)
{
    const uint8_t count = settings_device_count(g_settings);
    const char *hosts[SNAPSHOT_SLOTS] = { g_settings.bitaxeHost, g_settings.bitaxeHost2 };

    health.deviceCount = count;

    // Os dois na MESMA conexao: reconectar por dispositivo custaria outro ciclo
    // de radio inteiro, que e o gasto que domina a bateria.
    for (uint8_t i = 0; i < count; i++) {
        BitaxeStatus fresh;
        int httpCode = 0;
        const FetchResult r = bitaxe_fetch(hosts[i], g_settings.httpTimeoutMs, fresh, httpCode);

        // O mesmo fato serve a dois consumidores com vidas diferentes: o
        // watchdog so precisa dele ate o fim da chamada, o desenho precisa dele
        // ate o painel fechar, porque a alternancia redesenha sem rede.
        health.online[i] = g_justFetched[i] = (r == FETCH_OK);
        health.hashRate[i] = r == FETCH_OK ? fresh.hashRate10m : 0.0f;

        if (r == FETCH_OK) {
            snapshot_save(i, fresh, nowEpoch());
            g_note[i][0] = '\0';
        } else {
            // O cache do dispositivo que falhou e preservado; so a nota muda.
            // fetchNote devolve buffer estatico: copiar AQUI, antes da proxima volta.
            snprintf(g_note[i], sizeof(g_note[i]), "%s", fetchNote(r, httpCode));
        }
    }
}

static void enterPanel()
{
    screen_brightness(g_settings.brightnessPanel, true);

    const uint8_t count = settings_device_count(g_settings);

    // Zera o estado da sessao anterior do painel antes do primeiro desenho:
    // g_note nao pode mostrar um aviso remedido nesta abertura (a sessao
    // anterior pode ter terminado em "sem resposta" sobre um estado que esta
    // sendo checado de novo agora), e g_justFetched nao pode sobreviver a
    // sessao que acabou de fechar.
    for (uint8_t i = 0; i < SNAPSHOT_SLOTS; i++) {
        g_note[i][0] = '\0';
        g_justFetched[i] = false;
    }

    // Pinta o cache antes de qualquer coisa de rede: é isso que troca uma
    // espera de 3 s por um número na tela com a idade explícita.
    drawDevice(0);

    // batteryNow() já existe (main.cpp, criado no fecho da Fase 1): é o leitor
    // único que o relógio, o painel e a config compartilham. Não voltar a ler
    // getBattPercentage() cru aqui — foi exatamente a divergência corrigida lá.
    const int battery = batteryNow();
    if (battery >= 0 && battery < (int) g_settings.batteryMinPct) {
        // Conectar é de longe a operação mais cara; preserva o relógio.
        for (uint8_t i = 0; i < count; i++) {
            snprintf(g_note[i], sizeof(g_note[i]), "%s", "bateria baixa");
        }
        drawDevice(0);
        return;
    }

    if (!net_connect(g_settings.wifiSsid, g_settings.wifiPass, g_settings.wifiTimeoutMs)) {
        for (uint8_t i = 0; i < count; i++) {
            snprintf(g_note[i], sizeof(g_note[i]), "%s", "sem rede");
        }
        drawDevice(0);
        net_disconnect();
        return;
    }

    if (!g_timeIsValid && net_sync_time(g_settings.tzMinutes)) {
        struct tm t;
        if (getLocalTime(&t, 1000)) {
            watch->rtc->setDateTime(t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min,
                                    t.tm_sec);
            g_timeIsValid = true;
        }
    }

    WatchdogInputs health = {};
    pollDevices(health);
    net_disconnect();

    if (watchdog_should_alert(health)) {
        alertBuzz(1);
    }

    const int target = g_panelDevice == PANEL_CYCLE_CLOCK ? 0 : g_panelDevice;
    drawDevice(target);
}

static void enterConfig()
{
    screen_brightness(g_settings.brightnessPanel, true);

    if (!config_portal_begin(g_settings)) {
        config_view_message(watch, "falha ao subir", "o ponto de acesso");
        delay(2000);
        state = STATE_RELOGIO;
        // Voltar ao relogio devolve o brilho do relogio: sem isto a tela fica no
        // nivel do painel ate a proxima transicao, gastando mais justamente no
        // caminho em que o aparelho ja falhou em alguma coisa.
        screen_brightness(g_settings.brightnessClock, true);
        clock_view_draw(watch, clockModel());
        return;
    }

    const int battery = batteryNow();
    const bool lowBattery = (battery >= 0 && battery < (int) g_settings.batteryMinPct);

    lastCountdown = IDLE_TIMEOUT_CONFIG_MS / 1000;
    config_view_draw(watch, CONFIG_AP_SSID, CONFIG_AP_PASS, CONFIG_AP_URL, lastCountdown,
                     lowBattery);
}

static void goToSleep()
{
    // Dormir com o AP no ar deixaria o rádio ligado no estado mais caro.
    config_portal_end();

    // O despertar reexecuta o setup() e o painel começa fechado; deixar o
    // índice velho aqui faria o primeiro toque cair no dispositivo errado.
    g_panelDevice = PANEL_CYCLE_CLOCK;
    clearJustFetched();

    watch->closeBL();
    watch->displaySleep();

    // Duas fontes de despertar, conforme a spec: o toque e o botão PEK.
    // Os dois IRQs (TOUCH_INT e AXP202_INT) são independentes entre si e
    // ambos ativos em nível baixo (attachInterrupt(..., FALLING) na própria
    // biblioteca, pull-up em repouso). No ESP32 clássico (não S3), o modo
    // ext1 só tem duas opções: ALL_LOW (desperta só quando TODOS os pinos da
    // máscara estão em baixo ao mesmo tempo) ou ANY_HIGH (desperta quando
    // QUALQUER pino sobe). Nenhuma delas resolve "toque OU botão" para dois
    // pinos independentes ativos em baixo: ALL_LOW exigiria as duas fontes
    // simultâneas, e ANY_HIGH exigiria nível alto, que é o repouso, não o
    // evento. Por isso o despertar vai split em duas fontes independentes,
    // que o ESP32 já OR: ext0 (só aceita 1 pino, nível 0) para o toque, e
    // ext1 com máscara de 1 pino só (ALL_LOW com 1 bit é, trivialmente,
    // "esse pino em baixo") para o botão PEK.
    //
    // O botão PEK só derruba AXP202_INT se o IRQ correspondente estiver
    // habilitado no AXP202 — watch->begin() não faz isso sozinho (confirmado
    // nos exemplos da biblioteca, que habilitam explicitamente). Habilitado
    // uma vez no setup(); aqui só limpa o status pendente para o pino
    // começar o sono em nível alto (repouso) e não disparar na hora.
    watch->power->clearIRQ();

    esp_sleep_enable_ext0_wakeup((gpio_num_t) TOUCH_INT, 0);
    esp_sleep_enable_ext1_wakeup(1ULL << AXP202_INT, ESP_EXT1_WAKEUP_ALL_LOW);

    // Terceira fonte, independente das duas acima: o timer nao disputa modo com
    // ext0/ext1, entao convive com o toque e com o botao PEK sem custo.
    if (watchdog_should_run(g_settings.watchIntervalMin, batteryNow(), g_settings.batteryMinPct)) {
        esp_sleep_enable_timer_wakeup((uint64_t) g_settings.watchIntervalMin * 60ULL * 1000000ULL);
    }

    esp_deep_sleep_start();
}

// Ciclo de vigia: acontece com a tela APAGADA. Consulta os aparelhos, atualiza
// o cache e vibra se algo caiu. O usuario percebe pela vibracao, ou pelos dados
// ja frescos quando abrir o painel depois.
static void runWatchdogCycle()
{
    if (!net_connect(g_settings.wifiSsid, g_settings.wifiPass, g_settings.wifiTimeoutMs)) {
        // Sem rede nao ha o que afirmar sobre os mineradores: vibrar aqui seria
        // alarme falso toda vez que o WiFi oscilasse.
        net_disconnect();
        return;
    }

    WatchdogInputs health = {};
    pollDevices(health);
    net_disconnect();

    if (watchdog_should_alert(health)) {
        // Duas vibracoes: distingue do alerta de uma so que o painel da ao
        // abrir, e e perceptivel com o aparelho no pulso e a tela apagada.
        alertBuzz(2);

        // Quem desliga o motor e o Ticker que Motor::onec() arma para 200 ms
        // depois. No painel ele dispara sozinho porque o aparelho segue
        // acordado; aqui o proximo passo e deep sleep, que mataria o Ticker
        // antes da hora e deixaria o motor ligado ao dormir. Esperar o ultimo
        // pulso terminar custa 250 ms uma vez, e so quando ha alerta.
        delay(250);
    }
}

void setup()
{
    Serial.begin(115200);

    watch = TTGOClass::getWatch();
    watch->begin();

    // Antes de qualquer desenho: as views todas pintam no canvas que isto cria.
    if (!screen_begin(watch)) {
        Serial.println("sprite indisponivel: desenho direto no display");
    }

    settings_load(g_settings);
    Serial.printf("config: rede=%s host=%s tz=%d\n", g_settings.wifiSsid,
                  g_settings.bitaxeHost, (int) g_settings.tzMinutes);

    // Sem habilitar o ADC1 do AXP202 a leitura de bateria não vale nada.
    watch->power->adc1Enable(AXP202_BATT_VOL_ADC1 | AXP202_BATT_CUR_ADC1, true);

    // O botão PEK só gera IRQ (e só derruba AXP202_INT) se isto for
    // habilitado explicitamente — watch->begin() não faz isso. Reexecutado a
    // cada boot (o deep sleep passa de novo pelo setup()), mas é idempotente.
    watch->power->enableIRQ(AXP202_PEK_SHORTPRESS_IRQ, true);
    watch->power->clearIRQ();

    // O motor precisa ser inicializado antes do primeiro shake(); begin() nao
    // faz isso sozinho.
    watch->motor_begin();

    // O contador de passos do BMA423 nao vem ligado; begin() so inicializa o
    // acelerometro. Idempotente a cada boot.
    watch->bma->enableFeature(BMA423_STEP_CNTR, true);

    // Despertar por timer nao acende a tela: e a vigia rodando em segundo
    // plano. Precisa vir DEPOIS do ADC (goToSleep le a bateria para decidir se
    // rearma o timer) e do motor (o alerta vibra), e ANTES do openBL — acender
    // a tela para ninguem ver e justamente o gasto que a vigia evita.
    // goToSleep() nao retorna.
    if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER) {
        runWatchdogCycle();
        goToSleep();
    }

    watch->openBL();
    screen_brightness(g_settings.brightnessClock, false);

    // Confirmação de PSRAM: pendência aberta desde o firmware anterior, cujo
    // log de bootloader estava silenciado.
    Serial.printf("PSRAM presente: %s\n", psramFound() ? "sim" : "nao");
    Serial.printf("PSRAM total   : %u bytes\n", (unsigned) ESP.getPsramSize());
    Serial.printf("PSRAM livre   : %u bytes\n", (unsigned) ESP.getFreePsram());
    Serial.printf("heap livre    : %u bytes\n", (unsigned) ESP.getFreeHeap());

    state = state_next(STATE_DEEP_SLEEP, EVENT_WAKE, 0, idleTimeouts());
    clock_view_draw(watch, clockModel());
    lastInteraction = millis();
}

void loop()
{
    int16_t tx = 0, ty = 0;
    const bool touched = watch->getTouch(tx, ty);

    if (touched) {
        // A posição precisa ser copiada ANTES de drenar o toque: o laço abaixo
        // chama getTouch de novo e sobrescreve tx/ty, e a última leitura (a que
        // falha) não tem coordenada válida.
        const int16_t px = tx;
        const int16_t py = ty;

        int16_t dx = 0, dy = 0;
        while (watch->getTouch(dx, dy)) {
            delay(20);
        }

        AppEvent ev = EVENT_TOUCH;
        int nextDevice = g_panelDevice;

        if (state == STATE_RELOGIO && clock_hit(px, py) == HIT_CONFIG) {
            ev = EVENT_TOUCH_CONFIG;
        } else if (state == STATE_PAINEL) {
            // No painel o toque avança o ciclo; só o fim dele volta ao relógio.
            nextDevice = panel_cycle_next(g_panelDevice, settings_device_count(g_settings));
            ev = nextDevice == PANEL_CYCLE_CLOCK ? EVENT_TOUCH : EVENT_TOUCH_NEXT;
        }

        const AppState previous = state;
        state = state_next(state, ev, 0, idleTimeouts());

        if (state != STATE_CONFIG) {
            // No-op se o portal não estiver no ar.
            config_portal_end();
        }

        if (state == STATE_PAINEL) {
            if (previous == STATE_PAINEL) {
                // Alternar é redesenho puro: os dois snapshots já vieram na
                // mesma conexão quando o painel abriu.
                g_panelDevice = nextDevice;
                drawDevice(g_panelDevice);
            } else {
                g_panelDevice = 0;
                enterPanel();
            }
        } else if (state == STATE_CONFIG) {
            g_panelDevice = PANEL_CYCLE_CLOCK;
            clearJustFetched();
            enterConfig();
        } else {
            g_panelDevice = PANEL_CYCLE_CLOCK;
            clearJustFetched();
            screen_brightness(g_settings.brightnessClock, true);
            clock_view_draw(watch, clockModel());
        }
        lastInteraction = millis();
        return;
    }

    if (state == STATE_CONFIG) {
        // Requisição atendida conta como interação: sem isso o relógio dorme
        // no meio da digitação, já que o usuário não está tocando nele.
        if (config_portal_poll()) {
            lastInteraction = millis();
        }

        if (config_portal_should_restart()) {
            config_view_message(watch, "salvo", "reiniciando");
            // O cache e a hora ficaram obsoletos se o IP ou o fuso mudaram, e a
            // RTC memory sobrevive a reset por software — limpar é obrigatório.
            snapshot_clear();
            g_timeIsValid = false;
            delay(1000);  // dá tempo de a resposta HTTP sair pela rede
            config_portal_end();
            ESP.restart();
        }

        const uint32_t elapsed = millis() - lastInteraction;
        const uint32_t left =
            elapsed >= IDLE_TIMEOUT_CONFIG_MS ? 0 : (IDLE_TIMEOUT_CONFIG_MS - elapsed) / 1000;
        if (left != lastCountdown) {
            lastCountdown = left;
            config_view_tick(watch, left);
        }
    }

    state = state_next(state, EVENT_TICK, millis() - lastInteraction, idleTimeouts());
    if (state == STATE_DEEP_SLEEP) {
        goToSleep();
    }

    // O modo de config precisa de laço curto: na Task 7 é aqui que o servidor
    // web é atendido.
    delay(state == STATE_CONFIG ? 5 : 50);
}
