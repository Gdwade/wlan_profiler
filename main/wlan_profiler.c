/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"


#include "structs.h"
#include "linked_list.h"

#define GPIO_INPUT_IO_12     12
#define GPIO_INPUT_IO_13     13
#define GPIO_INPUT_PIN_SEL  ((1ULL<<GPIO_INPUT_IO_12) | (1ULL<<GPIO_INPUT_IO_13))
#define ASCII_ZERO_DEC 48
#define ASCII_A_DEC 65

#define ESP_WIFI_SSID "esp_tutorial"
#define ESP_WIFI_PASS "grayson_test"
#define ESP_WIFI_CHANNEL 1
#define MAX_STA_CONN 2

#define MAX_LIST_SIZE 64
#define BROADCAST 0xFFFFFFFFFFFF 

uint16_t active_devices;
uint8_t active_APs;
list * mac_list;
enum {CLI_TO_AP, AP_TO_CLI, MGMT};

void channel_hop_task(void *pvParameter)
{
    uint8_t channels[] = {1, 6, 11};
    uint8_t index = 0;

    while (true) {
        esp_wifi_set_channel(channels[index], WIFI_SECOND_CHAN_NONE);

        index = (index + 1) % 3;

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void log_wifi_ieee80211_h(struct wifi_ieee80211_h * wifi_h, uint8_t verbosity);

uint8_t *binary_to_hex(void *data, ssize_t n) {
    int location = 0;
    // n*3 because each uint8_t has two digits of hex and a space or newline.
    int hex_out_size = n*3;
    uint8_t *hex_str;
    if ((hex_str = malloc(hex_out_size)) == NULL) {
        return NULL;
    }

    if (data == NULL) {
        return NULL;
    }

    for (int i = 0; i<n; i++) {
        // uint8_t * so that pointer arithmatic works, then dereference
	int dec = *((uint8_t *) data + i);
	uint8_t first_hex;
	int first_dig = (dec  >> 4);
	if (first_dig < 10) {
	    first_hex = (uint8_t) (first_dig + ASCII_ZERO_DEC);
	} else {
	    first_hex = (uint8_t) ((first_dig-10) + ASCII_A_DEC);
	}	
	uint8_t second_hex;
        int rem = dec % 16;
        if (rem < 10) {
    	    second_hex = (uint8_t) (rem + ASCII_ZERO_DEC);
        } else {
    	    second_hex = (uint8_t) ((rem-10) + ASCII_A_DEC);
        }

	hex_str[location] = first_hex;
	hex_str[location + 1] = second_hex;
        

        if (i == n-1) {
            hex_str[location + 2] = '\n';
            return hex_str;
        }

        if ((i+1) % 16 != 0) {
    	    hex_str[location + 2] = ' ';
        } else {
            hex_str[location + 2] = '\n';
        }
        location += 3;
    }
    return hex_str;
}

static void wifi_sniffer_cb(void *buf, wifi_promiscuous_pkt_type_t type)
{
    wifi_promiscuous_pkt_t *pkt = (wifi_promiscuous_pkt_t *)buf;
    wifi_pkt_rx_ctrl_t *rx_ctrl = &pkt->rx_ctrl;    

    if (rx_ctrl->sig_mode != 0) return;

    struct wifi_ieee80211_h * wifi_h = (struct wifi_ieee80211_h *) pkt->payload;
    // 802.11 headers are Little Endian. 
    // Frame Control bits are easier to handle by casting to bytes.
    uint8_t *fc = (uint8_t *)&wifi_h->frame_ctrl;
    uint8_t verbosity = 0;
    uint8_t h_type    = (fc[0] & 0x0C) >> 2;
    uint8_t subtype = (fc[0] & 0xF0) >> 4;
    
    // DS Flags are in the second byte of Frame Control
    uint8_t to_ds   = (fc[1] & 0x01); 
    uint8_t from_ds = (fc[1] & 0x02) >> 1;

    // Filter by Type (we only want Data (2))
    //if (h_type != 2) return;

    // The "Base" MAC vs. the "BSSID"

    if (to_ds == 1 && from_ds == 0) {
        // Client -> AP
        // addr1: BSSID (AP), addr2: SA (Client), addr3: DA (Destination)
        log_wifi_ieee80211_h(wifi_h, CLI_TO_AP);
        uint64_t mac = 0;
        memcpy(&mac, wifi_h->addr2, 6);
        if (mac != 0xFFFFFFFFFFFFULL) {
            add_node(mac_list, mac);
        }
    } else if (to_ds == 0 && from_ds == 1) {
        // AP -> Client
        // addr1: DA (Client), addr2: BSSID (AP), addr3: SA (Source)
        log_wifi_ieee80211_h(wifi_h, AP_TO_CLI);
        uint64_t mac = 0;
        memcpy(&mac, wifi_h->addr1, 6);
        if (mac != 0xFFFFFFFFFFFFULL) {
            add_node(mac_list, mac);
        }

    } else if (to_ds == 0 && from_ds == 0 && verbosity >= 1) {
        // Management or Ad-Hoc
        //log_wifi_ieee80211_h(wifi_h, MGMT);
    }
    
}

/* 
 * Logs IEEE80211 wifi packet
 * type 1: 
 */
void log_wifi_ieee80211_h(struct wifi_ieee80211_h * wifi_h, uint8_t type) {
    switch(type) {
        case CLI_TO_AP:
            printf("DATA [Client -> AP] From: %02X:%02X:%02X:%02X:%02X:%02X To: %02X:%02X:%02X:%02X:%02X:%02X (via AP: %02X:%02X:%02X:%02X:%02X:%02X)\n",
               wifi_h->addr2[0], wifi_h->addr2[1], wifi_h->addr2[2], wifi_h->addr2[3], wifi_h->addr2[4], wifi_h->addr2[5],
               wifi_h->addr3[0], wifi_h->addr3[1], wifi_h->addr3[2], wifi_h->addr3[3], wifi_h->addr3[4], wifi_h->addr3[5],
               wifi_h->addr1[0], wifi_h->addr1[1], wifi_h->addr1[2], wifi_h->addr1[3], wifi_h->addr1[4], wifi_h->addr1[5]);
            break;
        case AP_TO_CLI:
            printf("DATA [AP -> Client] From: %02X:%02X:%02X:%02X:%02X:%02X To: %02X:%02X:%02X:%02X:%02X:%02X (via AP: %02X:%02X:%02X:%02X:%02X:%02X)\n",
               wifi_h->addr3[0], wifi_h->addr3[1], wifi_h->addr3[2], wifi_h->addr3[3], wifi_h->addr3[4], wifi_h->addr3[5],
               wifi_h->addr1[0], wifi_h->addr1[1], wifi_h->addr1[2], wifi_h->addr1[3], wifi_h->addr1[4], wifi_h->addr1[5],
               wifi_h->addr2[0], wifi_h->addr2[1], wifi_h->addr2[2], wifi_h->addr2[3], wifi_h->addr2[4], wifi_h->addr2[5]);
            break;
        case MGMT:
            printf("MGMT/Direct From: %02X:%02X:%02X:%02X:%02X:%02X To: %02X:%02X:%02X:%02X:%02X:%02X\n",
               wifi_h->addr2[0], wifi_h->addr2[1], wifi_h->addr2[2], wifi_h->addr2[3], wifi_h->addr2[4], wifi_h->addr2[5],
               wifi_h->addr1[0], wifi_h->addr1[1], wifi_h->addr1[2], wifi_h->addr1[3], wifi_h->addr1[4], wifi_h->addr1[5]);
            break;
        default:
            printf("log_wifi_ieee80211_h received invalid type");
            break;
    }
}

void app_main(void)
{
    printf("Sensing buttons!\n");

    //zero-initialize the config structure.
    gpio_config_t io_conf = {};
    //disable interrupt
    io_conf.intr_type = GPIO_INTR_DISABLE;

    //input mode
    io_conf.mode = GPIO_MODE_INPUT;

    //Pins 12 and 13 to input
    io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL;

    //enable pull down resistor
    io_conf.pull_down_en = GPIO_PULLDOWN_ENABLE;

    // Set pins
    gpio_config(&io_conf);

    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize TCP/IP
    ESP_ERROR_CHECK(esp_netif_init());

    // Create event loop
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Create default wifi interfaces
    esp_netif_create_default_wifi_ap();
    esp_netif_create_default_wifi_sta();

    // Initialize Wifi with defualt configs
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Set up wifi as access point and as a station
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_NULL));
    
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_channel(6, WIFI_SECOND_CHAN_NONE));

    esp_wifi_set_promiscuous(true);
    mac_list = init_list(MAX_LIST_SIZE);
    xTaskCreate(channel_hop_task, "channel_hop_task", 2048, NULL, 1,NULL);

    

    esp_wifi_set_promiscuous_rx_cb(wifi_sniffer_cb);

    while (true) {
        //int blue_button = gpio_get_level(GPIO_INPUT_IO_12);
        //int yellow_button = gpio_get_level(GPIO_INPUT_IO_13);
        //printf("Yellow: %d, Blue %d\n",yellow_button, blue_button);
        // Delay 500 ms (0.5s)
        uint8_t dev_on_wlan = get_list_size (mac_list);
        printf("# Connected Devices: %d\n", dev_on_wlan);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

}

