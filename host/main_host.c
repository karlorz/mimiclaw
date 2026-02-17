#include "mimi_config.h"
#include "platform/config_host.h"
#include "platform/platform_paths.h"

#include "bus/message_bus.h"
#include "agent/agent_loop.h"
#include "memory/memory_store.h"
#include "memory/session_mgr.h"
#include "llm/llm_proxy.h"
#include "tools/tool_registry.h"
#include "gateway/ws_server.h"
#include "channel/telegram_host.h"

#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <curl/curl.h>
#include "esp_err.h"
#include "esp_log.h"

static const char *TAG = "mimi_host";
static volatile sig_atomic_t s_running = 1;

static void on_signal(int signo)
{
    (void)signo;
    s_running = 0;
}

static void *outbound_dispatch_thread(void *arg)
{
    (void)arg;
    ESP_LOGI(TAG, "Outbound dispatch started");

    while (s_running) {
        mimi_msg_t msg;
        if (message_bus_pop_outbound(&msg, 500) != ESP_OK) {
            continue;
        }

        if (strcmp(msg.channel, MIMI_CHAN_WEBSOCKET) == 0) {
            ws_server_send(msg.chat_id, msg.content);
        } else if (strcmp(msg.channel, MIMI_CHAN_TELEGRAM) == 0) {
            telegram_host_send(msg.chat_id, msg.content);
        } else {
            ESP_LOGW(TAG, "Unsupported host channel: %s", msg.channel);
        }

        free(msg.content);
    }

    return NULL;
}

int main(int argc, char **argv)
{
    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);

    srand((unsigned)time(NULL));

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  MimiClaw Host - Linux/macOS Daemon");
    ESP_LOGI(TAG, "========================================");

    if (host_config_load(argc, argv) != MIMI_OK) {
        ESP_LOGE(TAG, "Failed to load host config");
        return 1;
    }

    const host_config_t *cfg = host_config_get();
    if (platform_paths_init(cfg->state_root) != MIMI_OK) {
        ESP_LOGE(TAG, "Failed to initialize path mapping");
        return 1;
    }

    curl_global_init(CURL_GLOBAL_DEFAULT);

    ESP_ERROR_CHECK(message_bus_init());
    ESP_ERROR_CHECK(memory_store_init());
    ESP_ERROR_CHECK(session_mgr_init());
    ESP_ERROR_CHECK(llm_proxy_init());
    ESP_ERROR_CHECK(tool_registry_init());
    ESP_ERROR_CHECK(agent_loop_init());
    ESP_ERROR_CHECK(telegram_host_init());

    ESP_ERROR_CHECK(agent_loop_start());
    ESP_ERROR_CHECK(ws_server_start());
    ESP_ERROR_CHECK(telegram_host_start());

    pthread_t outbound_thread;
    if (pthread_create(&outbound_thread, NULL, outbound_dispatch_thread, NULL) != 0) {
        ESP_LOGE(TAG, "Failed to start outbound thread");
        ws_server_stop();
        curl_global_cleanup();
        return 1;
    }

    ESP_LOGI(TAG, "Host daemon ready. WebSocket on %s:%u telegram=%s",
             cfg->ws_bind, (unsigned)cfg->ws_port,
             cfg->channel_telegram_enabled ? "enabled" : "disabled");

    while (s_running) {
        sleep(1);
    }

    ws_server_stop();
    telegram_host_stop();
    pthread_join(outbound_thread, NULL);
    curl_global_cleanup();

    ESP_LOGI(TAG, "Host daemon stopped");
    return 0;
}
