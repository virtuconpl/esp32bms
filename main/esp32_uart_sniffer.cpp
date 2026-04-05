#include <driver/uart.h>
#include <driver/usb_serial_jtag.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <stdio.h>
#include <string.h>
#include <esp_err.h>

#define UART_PORT_NUM       UART_NUM_1
#define UART_TX_PIN         4
#define UART_RX_PIN         5
#define UART_BAUD_RATE      38400

#define PREAMBULE_LEN       4
#define PREAMBULE_BYTES     {0x68, 0x31, 0xCE, 0x68}
#define FRAME_END           0x16
#define MAX_FRAME_SIZE      128

static const char* TAG __attribute__((unused)) = "BMS_SNIFFER";

// --- HEX dump ---
void log_bytes(const uint8_t* buf, size_t len) 
{
    printf("[HEX] ");
    for (size_t i = 0; i < len; i++) {
        printf("%02X ", buf[i]);
    }
    printf("\n");
}

// --- DEKODER DANYCH BMS ---
void decode_frame(const uint8_t* frame, size_t len) 
{
    if (len < 10) {
        printf("[DECODE] Za krótka ramka\n");
        return;
    }

    size_t pos = 4;
    uint8_t cmd = frame[pos++];
    uint8_t data_len = frame[pos++];

    printf("[DECODE] CMD: 0x%02X, DataLen: %u\n", cmd, data_len);

    if (data_len + 7 > len) {
        printf("[DECODE] Niepoprawna długość danych\n");
        return;
    }

    const uint8_t* data = &frame[6];

    printf("Napięcia sekcji:\n");
    for (int i = 0; i < 13; i++) {
        uint16_t raw = data[i*2] | (data[i*2 + 1] << 8);
        printf("  S%02d = %.3f V\n", i+1, raw / 1000.0);
    }

    size_t p = 13 * 2;
    uint16_t total = data[p] | (data[p+1] << 8);
    printf("Całkowite napięcie: %.3f V\n", total / 1000.0);
    p += 2;

    int16_t curr = (int16_t)(data[p] | (data[p+1] << 8));
    printf("Prąd: %.2f A\n", curr / 100.0);
    p += 2;

    uint8_t soc = data[p++];
    printf("SOC: %u %%\n", soc);

    int16_t t1 = (int16_t)(data[p] | (data[p+1] << 8));
    p += 2;
    int16_t t2 = (int16_t)(data[p] | (data[p+1] << 8));

    printf("Temperatury: T1=%.1f C, T2=%.1f C\n", t1 / 10.0, t2 / 10.0);
    printf("------------------------------------\n");
}

// --- ANALIZA STRUMIENIA UART ---
void process_buffer(uint8_t* buf, size_t len) 
{
    static uint8_t frame[MAX_FRAME_SIZE];
    static size_t frame_pos = 0;
    static bool in_frame = false;

    for (size_t i = 0; i < len; i++) {
        uint8_t b = buf[i];

        if (!in_frame) {
            static const uint8_t preamble[PREAMBULE_LEN] = PREAMBULE_BYTES;
            static size_t preamble_pos = 0;

            if (b == preamble[preamble_pos]) {
                preamble_pos++;
                if (preamble_pos == PREAMBULE_LEN) {
                    in_frame = true;
                    frame_pos = 0;
                    memcpy(frame, preamble, PREAMBULE_LEN);
                    frame_pos = PREAMBULE_LEN;
                    preamble_pos = 0;
                    printf("[DEBUG] Preambuła znaleziona!\n");
                }
            } else {
                preamble_pos = 0;
            }
        } else {
            frame[frame_pos++] = b;

            if (b == FRAME_END || frame_pos >= MAX_FRAME_SIZE) {
                printf("\n[BMS] Pakiet odebrany (%zu bajtów):\n", frame_pos);
                log_bytes(frame, frame_pos);
                decode_frame(frame, frame_pos);
                in_frame = false;
                frame_pos = 0;
            }
        }
    }
}

// --- UART INIT ---
void init_uart(void) 
{
    uart_config_t uart_config;
    memset(&uart_config, 0, sizeof(uart_config));
    
    uart_config.baud_rate = UART_BAUD_RATE;
    uart_config.data_bits = UART_DATA_8_BITS;
    uart_config.parity    = UART_PARITY_EVEN;
    uart_config.stop_bits = UART_STOP_BITS_1;
    uart_config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    uart_config.source_clk = UART_SCLK_DEFAULT;

    ESP_ERROR_CHECK(uart_param_config(UART_PORT_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_PORT_NUM, UART_TX_PIN, UART_RX_PIN,
                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(UART_PORT_NUM, 1024, 1024, 0, NULL, 0));
    
    printf("[UART] UART zainicjalizowany na pinach TX:%d RX:%d, baud:%d\n", 
           UART_TX_PIN, UART_RX_PIN, UART_BAUD_RATE);
}

// --- USB SERIAL INIT ---
void init_usb_serial(void)
{
    usb_serial_jtag_driver_config_t usb_serial_config = {
        .tx_buffer_size = 256,
        .rx_buffer_size = 256,
    };
    ESP_ERROR_CHECK(usb_serial_jtag_driver_install(&usb_serial_config));
    printf("[USB] USB Serial JTAG zainicjalizowany\n");
}

// --- MAIN ---
extern "C" void app_main(void) 
{
    init_usb_serial();
    init_uart();
    vTaskDelay(pdMS_TO_TICKS(100));

    const char* start_msg = "BMS UART sniffer started\n";
    usb_serial_jtag_write_bytes(start_msg, strlen(start_msg), portMAX_DELAY);
    
    printf("[MAIN] System uruchomiony, nasłuchuję na UART...\n");

    uint8_t buf[128];
    int total_bytes = 0;

    while (true) {
        int len = uart_read_bytes(UART_PORT_NUM, buf, sizeof(buf), 1000 / portTICK_PERIOD_MS);
        
        if (len > 0) {
            total_bytes += len;
            printf("[UART] Odebrano %d bajtów (łącznie: %d)\n", len, total_bytes);
            process_buffer(buf, len);
        } else {
            static int empty_count = 0;
            empty_count++;
            if (empty_count % 50 == 0) {
                printf("[UART] Brak danych... (czas oczekiwania: 1s)\n");
                empty_count = 0;
            }
        }
        
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}