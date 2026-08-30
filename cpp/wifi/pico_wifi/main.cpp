#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

extern "C"
{
#include "lwip/sockets.h"
#include "lwip/inet.h"
}

#define WIFI_SSID "YOUR_WIFI_NAME"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"

#define SERVER_IP "192.168.1.10" // PC IP
#define SERVER_PORT 8080

int main()
{
    stdio_init_all();
    sleep_ms(2000);

    if (cyw43_arch_init())
    {
        printf("WiFi init failed\n");
        return -1;
    }

    cyw43_arch_enable_sta_mode();

    printf("Connecting to WiFi...\n");
    if (cyw43_arch_wifi_connect_timeout_ms(
            WIFI_SSID,
            WIFI_PASSWORD,
            CYW43_AUTH_WPA2_AES_PSK,
            30000))
    {
        printf("WiFi connection failed\n");
        return -1;
    }

    printf("Connected to WiFi\n");

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        printf("Socket failed\n");
        return -1;
    }

    struct sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);

    printf("Connecting to server...\n");
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        printf("Connect failed\n");
        lwip_close(sock);
        return -1;
    }

    printf("Connected to server\n");

    const char *ws_handshake =
        "GET / HTTP/1.1\r\n"
        "Host: 192.168.1.10:8080\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: x3JJHMbDL1EzLkh9GBhXDw==\r\n"
        "Sec-WebSocket-Version: 13\r\n\r\n";

    send(sock, ws_handshake, strlen(ws_handshake), 0);

    char buffer[512];

    while (true)
    {
        int len = recv(sock, buffer, sizeof(buffer) - 1, 0);
        if (len > 0)
        {
            buffer[len] = 0;
            printf("Received:\n%s\n", buffer);
        }
        else
        {
            printf("Disconnected\n");
            break;
        }
        sleep_ms(1000);
    }

    lwip_close(sock);
    cyw43_arch_deinit();
    return 0;
}
