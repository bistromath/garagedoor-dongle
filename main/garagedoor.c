/* Hello World Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <netdb.h>
#include <sys/socket.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "esp_err.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "sodium.h"

#include "base64.h"
#include "wifi.h"

void send_message(void *p) {
    while(1) {
        // wait for callback to tell us that things are connected
        TickType_t xTicksToWait = 10000;
        EventBits_t ev = xEventGroupWaitBits(tcp_event_group, WIFI_CONNECTED_BIT, false, true, xTicksToWait);

        if(!(ev & WIFI_CONNECTED_BIT)) {
            ESP_LOGE("send_packet:", "Not connected after 10000 ticks!");
            continue;
        }

        // lookup hostname
        struct hostent *hp;
        hp = gethostbyname((const char *) CONFIG_GARAGEDOOR_SERVERNAME);

        if(hp == NULL) {
            ESP_LOGE("send_packet:", "Host not found");
            continue;
        }

        ESP_LOGI("send_packet:", "host IP: %s", inet_ntoa(hp->h_addr));

        //create socket
        struct sockaddr_in remote_addr;
        int sockfd, opt;

        sockfd = socket(AF_INET, SOCK_STREAM, 0);

        if(sockfd < 0) {
            ESP_LOGE("send_packet:", "error %i creating socket", sockfd);
            continue;
        }

        setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        memset(&remote_addr, 0, sizeof(struct sockaddr_in));
        memcpy(&remote_addr.sin_addr, hp->h_addr, sizeof(struct in_addr));
        remote_addr.sin_family = AF_INET;
        remote_addr.sin_port = htons(CONFIG_GARAGEDOOR_PORT);

        int status = connect(sockfd, (struct sockaddr *) &remote_addr, sizeof(remote_addr));

        if(status < 0) {
            ESP_LOGE("send_packet:", "error %i connecting to server", status);
            continue;
        }

        //send data
        char *msg = (char *) p;
        int sent_length = send(sockfd, msg, strlen(msg), 0);

        if(sent_length == strlen(msg)) {
            //we're good! let's get out of here
            xEventGroupSetBits(tcp_event_group, MESSAGE_SENT_BIT);
        }

        //hang around a while just to be sure
        vTaskDelay(1000 / portTICK_PERIOD_MS);

        //clean up
        close(sockfd);

        //do this only once
        vTaskDelete((TaskHandle_t *) NULL);
    }
}

/*void stop_sending_when_done(void *p) {
    while(1) {
        TickType_t xTicksToWait = 1000;
        EventBits_t ev = xEventGroupWaitBits(tcp_event_group, MESSAGE_SENT_BIT, false, true, xTicksToWait);

        if(!(ev & MESSAGE_SENT_BIT)) {
            ESP_LOGE("stop_sending_when_done:", "Not connected after 10000 ticks!");
            continue;
        }
        vTaskDelete((TaskHandle_t *) p);
    }
}
*/

void app_main()
{
    /* Print chip information */
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    printf("This is ESP32 chip with %d CPU cores, WiFi%s%s, ",
            chip_info.cores,
            (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
            (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "");

    printf("silicon revision %d, ", chip_info.revision);

    printf("%dMB %s flash\n", spi_flash_get_chip_size() / (1024 * 1024),
            (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    gd_wifi_init();

    unsigned char nonce[12];
    randombytes_buf(nonce, sizeof(nonce));
    unsigned char aad[] = {0x50, 0x51, 0x52, 0x53, 0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7};
    const unsigned char key[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
                                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
                                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    const unsigned char msg[] = "OPEN";
    unsigned char ciphertext[sizeof(msg) + crypto_aead_chacha20poly1305_IETF_ABYTES];
    unsigned long long ciphertext_len;

    crypto_aead_chacha20poly1305_ietf_encrypt(ciphertext, &ciphertext_len,
                                              msg, sizeof(msg),
                                              aad, sizeof(aad),
                                              NULL, nonce, key);

    // probably actually better to just concatenate everything and then b64 it.
    size_t raw_msg_len = (ciphertext_len + sizeof(aad) + sizeof(key));
    unsigned char rawmsg[raw_msg_len];
    memcpy(rawmsg, nonce, sizeof(nonce));
    memcpy(rawmsg+sizeof(nonce), aad, sizeof(aad));
    memcpy(rawmsg+sizeof(nonce)+sizeof(aad), ciphertext, ciphertext_len);

    size_t est_b64_len = (8*raw_msg_len)/6;
    unsigned char b64_msg[est_b64_len];
    size_t enc_len;
    base64_encode(b64_msg, rawmsg, sizeof(nonce)+sizeof(aad)+sizeof(ciphertext), &enc_len);
    b64_msg[enc_len++] = '\n';
    b64_msg[enc_len] = 0;

    //assemble a complete message
    char wire_msg[enc_len+15];
    memset(wire_msg, 0x00, enc_len+15);
    sprintf(wire_msg, "GARAGEMAGIC");
    strcpy(wire_msg+strlen(wire_msg), (char *) b64_msg);

    printf("Ciphertext output: %s\n", wire_msg);

    xTaskCreate(send_message, "SENDMESSAGE", 8192, (void *) wire_msg, tskIDLE_PRIORITY+1, NULL);
}
