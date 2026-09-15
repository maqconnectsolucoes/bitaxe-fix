#include <string.h>

#include "esp_log.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mqtt_client.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"

#include "connect.h"
#include "global_state.h"
#include "nvs_config.h"
#include "mqtt_task.h"
#include "system_api_json.h"

static const char *TAG = "mqtt_task";

#define MQTT_TOPIC_MAX_LEN (MQTT_DEVICE_ID_MAX_LEN + 32)
#define MQTT_RECONNECT_DELAY_MS 10000

// Uma queda de internet deixa o broker inalcançável por minutos. Sem limite, o
// cliente refaz handshake TLS sem parar, e o log (anel de ~33 min) enche de
// erro repetido -- apagando justamente o histórico necessário para diagnosticar
// qualquer outro problema ocorrido no período.
#define MQTT_FALHAS_ATE_DESISTIR 10
#define MQTT_LOG_A_CADA 10
// Intervalo do teste de alcance durante uma queda. Curto de propósito: é um
// connect TCP, ordens de grandeza mais barato que um handshake TLS, e define
// quanto tempo depois da internet voltar a publicação recomeça.
#define MQTT_TESTE_ALCANCE_MS 15000

static esp_mqtt_client_handle_t s_client = NULL;
static bool s_connected = false;
static int s_falhas_seguidas = 0;

// esp-mqtt does NOT copy the certificate (mqtt_client.h): the pointer handed to
// broker.verification.certificate has to stay valid for as long as the client
// lives, so this is kept here instead of on the stack of the setup function.
static char *s_ca_cert = NULL;

void mqtt_get_device_id(char *out, size_t out_len)
{
    if (!out || out_len == 0) return;

    char *configured = nvs_config_get_string(NVS_CONFIG_MQTT_DEVICE_ID);
    if (configured && configured[0]) {
        strlcpy(out, configured, out_len);
        free(configured);
        return;
    }
    free(configured);

    uint8_t mac[6] = {0};
    esp_wifi_get_mac(WIFI_IF_STA, mac);
    snprintf(out, out_len, "bitaxe-%02x%02x%02x%02x%02x%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

/**
 * @brief Testa se o broker aceita conexão TCP, sem TLS e sem MQTT.
 *
 * Usado durante uma queda de internet para decidir quando vale a pena recriar o
 * cliente. Um connect TCP não negocia certificado nem aloca buffers de sessão,
 * então pode ser repetido de minuto em minuto sem o custo que tornava a
 * reconexão automática destrutiva para o log.
 */
static bool mqtt_broker_alcancavel(void)
{
    char *host = nvs_config_get_string(NVS_CONFIG_MQTT_HOST);
    if (!host || !host[0]) {
        free(host);
        return false;
    }

    char porta[8];
    snprintf(porta, sizeof(porta), "%u", (unsigned) nvs_config_get_u16(NVS_CONFIG_MQTT_PORT));

    struct addrinfo dicas = {.ai_family = AF_UNSPEC, .ai_socktype = SOCK_STREAM};
    struct addrinfo *res = NULL;
    int rc = getaddrinfo(host, porta, &dicas, &res);
    free(host);
    if (rc != 0 || res == NULL) {
        if (res) freeaddrinfo(res);
        return false;   // sem DNS já basta para saber que a internet não voltou
    }

    bool ok = false;
    int fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (fd >= 0) {
        // Não-bloqueante + select: um connect bloqueante não respeita SO_SNDTIMEO
        // no lwip e poderia prender a tarefa por muito mais que o desejado.
        int flags = fcntl(fd, F_GETFL, 0);
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);

        if (connect(fd, res->ai_addr, res->ai_addrlen) == 0) {
            ok = true;
        } else if (errno == EINPROGRESS) {
            fd_set escrita;
            FD_ZERO(&escrita);
            FD_SET(fd, &escrita);
            struct timeval tv = {.tv_sec = 5, .tv_usec = 0};
            if (select(fd + 1, NULL, &escrita, NULL, &tv) > 0) {
                int erro = 0;
                socklen_t tam = sizeof(erro);
                ok = getsockopt(fd, SOL_SOCKET, SO_ERROR, &erro, &tam) == 0 && erro == 0;
            }
        }
        close(fd);
    }

    freeaddrinfo(res);
    return ok;
}

static void mqtt_event_handler(void *args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t) event_data;

    switch ((esp_mqtt_event_id_t) event_id) {
        case MQTT_EVENT_CONNECTED:
            s_connected = true;
            if (s_falhas_seguidas > 0) {
                ESP_LOGI(TAG, "Reconnected to broker after %d failed attempt(s)", s_falhas_seguidas);
            } else {
                ESP_LOGI(TAG, "Connected to broker");
            }
            s_falhas_seguidas = 0;
            break;
        case MQTT_EVENT_DISCONNECTED:
            s_connected = false;
            // Só a primeira e depois uma a cada MQTT_LOG_A_CADA: numa queda de
            // internet isto dispara a cada tentativa, e logar todas custa o
            // histórico inteiro do anel de log.
            if (s_falhas_seguidas % MQTT_LOG_A_CADA == 0) {
                ESP_LOGW(TAG, "Disconnected from broker (falha %d)", s_falhas_seguidas + 1);
            }
            s_falhas_seguidas++;
            break;
        case MQTT_EVENT_ERROR:
            // Never fatal: mining must not depend on the broker being reachable.
            if (event && event->error_handle && s_falhas_seguidas % MQTT_LOG_A_CADA == 0) {
                ESP_LOGW(TAG, "Broker error, type %d, tls stack err 0x%x", event->error_handle->error_type,
                         event->error_handle->esp_tls_last_esp_err);
            }
            break;
        default:
            break;
    }
}

static void mqtt_client_stop(void)
{
    if (s_client) {
        esp_mqtt_client_stop(s_client);
        esp_mqtt_client_destroy(s_client);
        s_client = NULL;
    }
    s_connected = false;
    s_falhas_seguidas = 0;
    free(s_ca_cert);
    s_ca_cert = NULL;
}

static bool mqtt_client_start(void)
{
    char *host = nvs_config_get_string(NVS_CONFIG_MQTT_HOST);
    if (!host || !host[0]) {
        ESP_LOGW(TAG, "No broker configured, not connecting");
        free(host);
        return false;
    }

    uint16_t port = nvs_config_get_u16(NVS_CONFIG_MQTT_PORT);
    char *user = nvs_config_get_string(NVS_CONFIG_MQTT_USER);
    char *pass = nvs_config_get_string(NVS_CONFIG_MQTT_PASS);
    s_ca_cert = nvs_config_get_string(NVS_CONFIG_MQTT_CA_CERT);

    char client_id[MQTT_DEVICE_ID_MAX_LEN];
    mqtt_get_device_id(client_id, sizeof(client_id));

    esp_mqtt_client_config_t cfg = {
        .broker.address.hostname = host,
        .broker.address.port = port,
        // A CA turns the transport into TLS; without one the broker is plain TCP.
        .broker.address.transport = (s_ca_cert && s_ca_cert[0]) ? MQTT_TRANSPORT_OVER_SSL : MQTT_TRANSPORT_OVER_TCP,
        .credentials.username = (user && user[0]) ? user : NULL,
        .credentials.client_id = client_id,
        .credentials.authentication.password = (pass && pass[0]) ? pass : NULL,
        .session.keepalive = 60,
        .network.disable_auto_reconnect = false,
        // Padrão do esp-mqtt é 10s; com o broker fora do ar isso são 6 handshakes
        // TLS por minuto sem utilidade nenhuma.
        .network.reconnect_timeout_ms = 30000,
        .network.timeout_ms = 10000,
    };
    if (s_ca_cert && s_ca_cert[0]) {
        cfg.broker.verification.certificate = s_ca_cert;
    }

    s_client = esp_mqtt_client_init(&cfg);
    bool ok = false;
    if (s_client) {
        esp_mqtt_client_register_event(s_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
        ok = esp_mqtt_client_start(s_client) == ESP_OK;
        if (!ok) {
            ESP_LOGE(TAG, "Failed to start client");
            esp_mqtt_client_destroy(s_client);
            s_client = NULL;
        }
    } else {
        ESP_LOGE(TAG, "Failed to init client");
    }

    // esp-mqtt copies hostname, username, password and client id; only the
    // certificate is referenced, so everything else can be released right away.
    free(host);
    free(user);
    free(pass);
    if (!ok) {
        free(s_ca_cert);
        s_ca_cert = NULL;
    }
    return ok;
}

static void mqtt_publish_telemetry(GlobalState *GLOBAL_STATE, const char *device_id, const char *topic)
{
    cJSON *root = system_api_get_telemetry_json(GLOBAL_STATE);
    if (!root) {
        ESP_LOGW(TAG, "Could not build telemetry payload");
        return;
    }
    cJSON_AddStringToObject(root, "deviceId", device_id);

    char *payload = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!payload) {
        ESP_LOGW(TAG, "Could not serialise telemetry payload");
        return;
    }

    if (esp_mqtt_client_publish(s_client, topic, payload, 0, 1, 0) < 0) {
        ESP_LOGW(TAG, "Publish to %s failed", topic);
    }
    free(payload);
}

void mqtt_task(void *pvParameters)
{
    GlobalState *GLOBAL_STATE = (GlobalState *) pvParameters;

    char device_id[MQTT_DEVICE_ID_MAX_LEN];
    char topic[MQTT_TOPIC_MAX_LEN];
    bool was_enabled = false;

    while (1) {
        bool enabled = nvs_config_get_bool(NVS_CONFIG_MQTT_ENABLED);

        if (!enabled) {
            if (was_enabled) {
                ESP_LOGI(TAG, "Publishing disabled, dropping connection");
                mqtt_client_stop();
                was_enabled = false;
            }
            vTaskDelay(pdMS_TO_TICKS(MQTT_RECONNECT_DELAY_MS));
            continue;
        }

        // The gate in main.c only proves the network was up at boot; it can drop
        // later, same reason stratum_v1_task revalidates on every pass.
        if (!wifi_is_connected()) {
            vTaskDelay(pdMS_TO_TICKS(MQTT_RECONNECT_DELAY_MS));
            continue;
        }

        if (!was_enabled) {
            // Read the identity once per connection so an edit in AxeOS is
            // picked up without needing a reboot.
            mqtt_get_device_id(device_id, sizeof(device_id));
            snprintf(topic, sizeof(topic), "bitaxe/%s/telemetry", device_id);

            if (!mqtt_client_start()) {
                vTaskDelay(pdMS_TO_TICKS(MQTT_RECONNECT_DELAY_MS));
                continue;
            }
            ESP_LOGI(TAG, "Publishing to %s", topic);
            was_enabled = true;
        }

        // Desiste depois de muitas falhas seguidas: destrói o cliente, libera os
        // recursos de TLS e espera antes de tentar de novo. Numa queda longa isso
        // troca dezenas de tentativas inúteis por uma a cada 5 minutos.
        if (s_falhas_seguidas >= MQTT_FALHAS_ATE_DESISTIR) {
            ESP_LOGW(TAG, "Broker unreachable after %d attempts, dropping client until the network returns",
                     s_falhas_seguidas);
            mqtt_client_stop();
            s_falhas_seguidas = 0;
            was_enabled = false;

            // Espera ativa: sonda TCP barata em vez de repetir TLS. Assim a
            // publicação recomeça em segundos quando a internet volta, sem
            // martelar o broker enquanto ela está fora.
            while (nvs_config_get_bool(NVS_CONFIG_MQTT_ENABLED)) {
                vTaskDelay(pdMS_TO_TICKS(MQTT_TESTE_ALCANCE_MS));
                if (wifi_is_connected() && mqtt_broker_alcancavel()) {
                    ESP_LOGI(TAG, "Broker reachable again, reconnecting");
                    break;
                }
            }
            continue;
        }

        if (s_connected) {
            mqtt_publish_telemetry(GLOBAL_STATE, device_id, topic);
        }

        uint16_t interval = nvs_config_get_u16(NVS_CONFIG_MQTT_INTERVAL);
        if (interval < 5) interval = 5;
        vTaskDelay(pdMS_TO_TICKS((uint32_t) interval * 1000));
    }
}
