#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "btstack.h"

/* ================= Advertising Data ================= */
// Flags + Shortened Name "Pico2"
static uint8_t adv_data[] = {
    0x02, 0x01, 0x06,
    0x06, 0x08,
    'P', 'i', 'c', 'o', '2'};

/* ================= Minimal GATT Database ================= */
static const uint8_t gatt_db[] = {
    // Primary Service: GAP (0x1800)
    0x0A, 0x00,
    0x02, 0x00,
    0x01,
    0x00, 0x18,

    // Characteristic: Device Name (0x2A00), Read
    0x0D, 0x00,
    0x03, 0x00,
    0x02,
    0x03, 0x00,
    0x00, 0x2A,
    0x02,

    // Value: "Pico2"
    0x09, 0x00,
    0x04, 0x00,
    'P', 'i', 'c', 'o', '2',

    // Primary Service: GATT (0x1801)
    0x07, 0x00,
    0x05, 0x00,
    0x01,
    0x01, 0x18};

/* ================= LED / State ================= */
static bool ble_connected = false;
static btstack_timer_source_t led_timer;

static void led_timer_handler(btstack_timer_source_t *ts)
{
    if (ble_connected)
    {
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1); // solid ON
    }
    else
    {
        static bool led = false;
        led = !led;
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, led); // blink
    }
    btstack_run_loop_set_timer(ts, 500);
    btstack_run_loop_add_timer(ts);
}

/* ================= BTstack Event Handler ================= */
static void packet_handler(uint8_t packet_type, uint16_t, uint8_t *packet, uint16_t)
{
    if (packet_type != HCI_EVENT_PACKET)
        return;

    uint8_t event = hci_event_packet_get_type(packet);

    switch (event)
    {
    case BTSTACK_EVENT_STATE:
    {
        uint8_t state = btstack_event_state_get_state(packet);
        printf("BTstack state = %u\n", state);

        if (state == HCI_STATE_WORKING)
        {
            printf("HCI working -> start advertising\n");

            // start LED timer now (proves BTstack run loop is alive)
            led_timer.process = &led_timer_handler;
            btstack_run_loop_set_timer(&led_timer, 500);
            btstack_run_loop_add_timer(&led_timer);

            gap_advertisements_set_params(
                0x00A0, 0x00A0,
                0, 0, NULL, 0x07, 0);
            gap_advertisements_set_data(sizeof(adv_data), adv_data);
            gap_advertisements_enable(1);

            printf("Advertising enabled\n");
        }
        break;
    }

    case HCI_EVENT_LE_META:
        if (hci_event_le_meta_get_subevent_code(packet) == HCI_SUBEVENT_LE_CONNECTION_COMPLETE)
        {
            ble_connected = true;
            printf("BLE connected\n");
        }
        break;

    case HCI_EVENT_DISCONNECTION_COMPLETE:
        ble_connected = false;
        printf("BLE disconnected\n");
        break;

    default:
        break;
    }
}

int main()
{
    stdio_init_all();
    sleep_ms(3000);

    printf("BOOT OK\n");

    if (cyw43_arch_init())
    {
        printf("CYW43 init failed\n");
        while (1)
            tight_loop_contents();
    }
    printf("CYW43 OK\n");

    // quick blink so you know firmware is alive before BTstack
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
    sleep_ms(100);
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);

    // BTstack init
    l2cap_init();
    sm_init();
    att_server_init(gatt_db, NULL, NULL);

    static btstack_packet_callback_registration_t cb;
    cb.callback = &packet_handler;
    hci_add_event_handler(&cb);

    printf("Powering on HCI\n");
    hci_power_control(HCI_POWER_ON);

    btstack_run_loop_execute();
}
