#ifndef DIYBMSWebServer_Json_Req_H_
#define DIYBMSWebServer_Json_Req_H_

#include <stdio.h>
#include <esp_http_server.h>
#include "settings.h"
#include "FS.h"
#include "LittleFS.h"
#include "SD.h"
#include <SPIFFS.h>
#include "time.h"
#include "defines.h"
#include "Rules.h"
#include "settings.h"
#include "ArduinoJson.h"
#include "PacketRequestGenerator.h"
#include "PacketReceiveProcessor.h"

#include "EmbeddedFiles_AutoGenerated.h"
#include "HAL_ESP32.h"

#include "CurrentMonitorINA229.h"
#include "history.h"

#include "ControllerCAN.h"

esp_err_t api_handler(httpd_req_t *req);
esp_err_t content_handler_downloadfile(httpd_req_t *req);
esp_err_t ha_handler(httpd_req_t *req);

esp_err_t SendFileInChunks(httpd_req_t *req, FS &filesystem, const char *filename);
int fileSystemListDirectory(char *buffer, size_t bufferLen, fs::FS &fs, const char *dirname, uint8_t levels);

extern diybms_eeprom_settings mysettings;
extern PacketRequestGenerator prg;
extern PacketReceiveProcessor receiveProc;
extern HAL_ESP32 hal;
extern fs::SDFS SD;

extern TaskHandle_t avrprog_task_handle;
extern uint32_t canbus_messages_received;
extern uint32_t canbus_messages_sent;
extern uint32_t canbus_messages_failed_sent;
extern uint32_t canbus_messages_received_error;

extern Rules rules;
extern ControllerState _controller_state;
extern void formatCurrentDateTime(char *buf, size_t buf_size);
extern void setNoStoreCacheControl(httpd_req_t *req);
extern char CookieValue[20 + 1];
extern std::string hostname;
extern std::string ip4_to_string(const uint32_t ipaddr);

extern uint32_t time100;
extern uint32_t time20;
extern uint32_t time10;
extern CurrentMonitorINA229 currentmon_internal;

extern History history;
extern wifi_eeprom_settings _wificonfig;
extern esp_err_t diagnosticJSON(httpd_req_t *req, char buffer[], int bufferLenMax);

extern bool mqttClient_connected;
extern uint16_t mqtt_error_connection_count;
extern uint16_t mqtt_error_transport_count;
extern uint16_t mqtt_connection_count;
extern uint16_t mqtt_disconnection_count;

extern uint16_t wifi_count_rssi_low;
extern uint16_t wifi_count_sta_start;
extern uint16_t wifi_count_sta_connected;
extern uint16_t wifi_count_sta_disconnected;
extern uint16_t wifi_count_sta_lost_ip;
extern uint16_t wifi_count_sta_got_ip;

extern bool wifi_isconnected;

extern ControllerCAN can;

#endif
