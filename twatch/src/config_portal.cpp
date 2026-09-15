#include "config_portal.h"

#include <DNSServer.h>
#include <WebServer.h>
#include <WiFi.h>

#include "settings_store.h"

const char *CONFIG_AP_SSID = "T-Watch-Setup";
const char *CONFIG_AP_PASS = "bitaxe1234";  // WPA2 exige 8 caracteres ou mais
const char *CONFIG_AP_URL = "http://192.168.4.1";

static DNSServer *dns = nullptr;
static WebServer *server = nullptr;
static Settings g_current;
static bool g_served = false;
static bool g_restart = false;

// O SSID da rede do usuário pode conter aspas e &, e ele volta para dentro de
// um atributo HTML. Sem escapar, um SSID com aspa quebra o formulário.
static String htmlEscape(const char *s)
{
    String out;
    for (const char *p = s; *p != '\0'; p++) {
        switch (*p) {
        case '&': out += "&amp;"; break;
        case '<': out += "&lt;"; break;
        case '>': out += "&gt;"; break;
        case '"': out += "&quot;"; break;
        case '\'': out += "&#39;"; break;
        default: out += *p; break;
        }
    }
    return out;
}

static String tzOptions(int16_t selected)
{
    String out;
    for (int m = SETTINGS_TZ_MIN_MINUTES; m <= SETTINGS_TZ_MAX_MINUTES; m += SETTINGS_TZ_STEP_MINUTES) {
        const int magnitude = m < 0 ? -m : m;
        char label[8];
        snprintf(label, sizeof(label), "%c%02d:%02d", m < 0 ? '-' : '+', magnitude / 60,
                 magnitude % 60);

        out += "<option value=";
        out += m;
        if (m == selected) {
            out += " selected";
        }
        out += ">UTC";
        out += label;
        out += "</option>";
    }
    return out;
}

static String numberField(const char *label, const char *name, uint32_t value)
{
    String out = "<label>";
    out += label;
    out += "<input name=";
    out += name;
    out += " type=number value=";
    out += value;
    out += "></label>";
    return out;
}

static void sendForm(const char *errorMsg)
{
    String h = F("<!doctype html><html lang=pt-br><meta charset=utf-8>"
                 "<meta name=viewport content='width=device-width,initial-scale=1'>"
                 "<title>T-Watch</title><style>"
                 "body{font-family:sans-serif;margin:16px;max-width:420px}"
                 "label{display:block;margin-top:12px;font-size:14px;color:#333}"
                 "input,select{width:100%;padding:8px;font-size:16px;box-sizing:border-box}"
                 "button{margin-top:20px;padding:12px;width:100%;font-size:16px}"
                 ".e{background:#fdd;border:1px solid #c00;padding:8px}"
                 "</style><h2>T-Watch</h2>");

    if (errorMsg != nullptr) {
        h += "<p class=e>";
        h += htmlEscape(errorMsg);
        h += "</p>";
    }

    h += "<form method=post action=/save>";

    h += "<label>Rede WiFi<input name=ssid maxlength=32 value='";
    h += htmlEscape(g_current.wifiSsid);
    h += "'></label>";

    // A senha gravada NUNCA volta para a página: este AP está ao alcance de
    // qualquer vizinho. Campo vazio significa "manter a atual".
    h += F("<label>Senha WiFi<input name=pass type=password maxlength=63 "
           "placeholder='(inalterada)'></label>");

    h += "<label>IP do Bitaxe<input name=host maxlength=15 value='";
    h += htmlEscape(g_current.bitaxeHost);
    h += "'></label>";

    h += "<label>IP do segundo Bitaxe (opcional)<input name=host2 maxlength=15 value='";
    h += htmlEscape(g_current.bitaxeHost2);
    h += "' placeholder='deixe vazio para um so'></label>";

    h += "<label>Fuso horario<select name=tz>";
    h += tzOptions(g_current.tzMinutes);
    h += "</select></label>";

    h += numberField("Timeout de WiFi (ms)", "wifims", g_current.wifiTimeoutMs);
    h += numberField("Timeout de HTTP (ms)", "httpms", g_current.httpTimeoutMs);
    h += numberField("Bateria minima (%)", "battmin", g_current.batteryMinPct);
    h += numberField("Ociosidade do relogio (ms)", "idlerel", g_current.idleRelogioMs);
    h += numberField("Ociosidade do painel (ms)", "idlepan", g_current.idlePainelMs);
    h += numberField("Brilho do relogio (%)", "brtclk", g_current.brightnessClock);
    h += numberField("Brilho do painel (%)", "brtpan", g_current.brightnessPanel);
    h += numberField("Vigia periodica (min, 0 desliga)", "watchmin", g_current.watchIntervalMin);

    h += F("<button type=submit>Salvar e reiniciar</button></form>");

    server->send(200, "text/html; charset=utf-8", h);
}

static void handleRoot()
{
    g_served = true;
    sendForm(nullptr);
}

static void handleSave()
{
    g_served = true;

    // As Strings precisam viver até o fim da função: SettingsForm guarda
    // ponteiros para dentro delas.
    const String ssid = server->arg("ssid");
    const String pass = server->arg("pass");
    const String host = server->arg("host");
    const String host2 = server->arg("host2");
    const String tz = server->arg("tz");
    const String wifims = server->arg("wifims");
    const String httpms = server->arg("httpms");
    const String battmin = server->arg("battmin");
    const String idlerel = server->arg("idlerel");
    const String idlepan = server->arg("idlepan");
    const String brtclk = server->arg("brtclk");
    const String brtpan = server->arg("brtpan");
    const String watchmin = server->arg("watchmin");

    SettingsForm f;
    f.ssid = ssid.c_str();
    f.pass = pass.c_str();
    f.host = host.c_str();
    f.host2 = host2.c_str();
    f.tzMinutes = tz.c_str();
    f.wifiTimeoutMs = wifims.c_str();
    f.httpTimeoutMs = httpms.c_str();
    f.batteryMinPct = battmin.c_str();
    f.idleRelogioMs = idlerel.c_str();
    f.idlePainelMs = idlepan.c_str();
    f.brightnessClock = brtclk.c_str();
    f.brightnessPanel = brtpan.c_str();
    f.watchIntervalMin = watchmin.c_str();

    Settings candidate = {};
    const SettingsError e = settings_apply_form(g_current, f, candidate);
    if (e != SETTINGS_OK) {
        sendForm(settings_error_message(e));
        return;
    }

    if (!settings_save(candidate)) {
        // Não reinicia: o estado anterior continua válido e utilizável.
        sendForm("Falha ao gravar na memoria do relogio. Tente de novo.");
        return;
    }

    g_current = candidate;
    g_restart = true;
    server->send(200, "text/html; charset=utf-8",
                 F("<!doctype html><html lang=pt-br><meta charset=utf-8>"
                   "<meta name=viewport content='width=device-width,initial-scale=1'>"
                   "<h2>Salvo</h2><p>O relogio esta reiniciando com a configuracao nova.</p>"));
}

static void handleNotFound()
{
    g_served = true;
    // Redirecionar tudo para a raiz é o que faz o celular reconhecer a rede
    // como portal cativo e abrir a página sozinho.
    server->sendHeader("Location", CONFIG_AP_URL, true);
    server->send(302, "text/plain", "");
}

bool config_portal_begin(const Settings &current)
{
    g_current = current;
    g_restart = false;
    g_served = false;

    WiFi.mode(WIFI_AP);
    if (!WiFi.softAP(CONFIG_AP_SSID, CONFIG_AP_PASS)) {
        WiFi.mode(WIFI_OFF);
        return false;
    }

    dns = new DNSServer();
    server = new WebServer(80);

    dns->start(53, "*", WiFi.softAPIP());

    server->on("/", HTTP_GET, handleRoot);
    server->on("/save", HTTP_POST, handleSave);
    server->onNotFound(handleNotFound);
    server->begin();

    return true;
}

bool config_portal_poll()
{
    if (server == nullptr) {
        return false;
    }
    g_served = false;
    dns->processNextRequest();
    server->handleClient();
    return g_served;
}

bool config_portal_should_restart()
{
    return g_restart;
}

void config_portal_end()
{
    // Saída antecipada de verdade: sem isso, chamar esta função no caminho
    // relógio -> painel desligaria o rádio logo antes de o painel precisar
    // dele.
    if (server == nullptr) {
        return;
    }

    server->stop();
    delete server;
    server = nullptr;

    if (dns != nullptr) {
        dns->stop();
        delete dns;
        dns = nullptr;
    }
    g_restart = false;
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
}
