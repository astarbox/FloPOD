//
// FLO POD controller
//
// Copyright © 2026 AStarBox. All rights reserved.
//

#include "Arduino.h"
#include <rtc_wdt.h>
#include <esp_task_wdt.h>
#include <Wire.h>

#include "config.h"
#include "powerManagement.h"
#include "motorCtrl.h"
#include "AlpacaAPI.h"
#include "podController.h"

String sLocalIPAdress = "";
PodConfig *globalPodConfig; // init GPIO, provide config management
WIFIConfig	wifiApConfig;
IPConfig	wifiClientConfig;
PowerConfig powerConfig;

// interrupt handlers
void openIntHandler();
void closeIntHandler();
void conditionIntHandler();
void buttonHandler();

// FreeRTOS task
void MotorTask(void *);
void PowerTask(void *);

// other object
esp_task_wdt_config_t twdt_config = {
	.timeout_ms = 1000000,
	.idle_core_mask = 0,    // Bitmask of cores
	.trigger_panic = false,
};

void setup()
{
    // configure all pins
	globalPodConfig = new PodConfig();
    // rtc_wdt_protect_off();
	esp_task_wdt_deinit();
	esp_task_wdt_init(&twdt_config);
	esp_task_wdt_add(NULL);
	disableCore0WDT();
	disableCore1WDT();

	// Start local hostspot and connect to local wifi if configured and available
	configureWiFi();

    // start I2C
    Wire.begin();

    // create tasks
    xTaskCreatePinnedToCore(MotorTask, "MotorTask", 10000, NULL, 8, NULL,  0); // priority 8 (medium) on Core 0
    xTaskCreatePinnedToCore(PowerTask, "PowerTask", 10000, NULL, 16, NULL,  0); // priority 16 (High) on Core 0

	// attach output OC alarm interrupt
	// OC_ALARM

	// MAIN_OC_ALARM

	// MAG_TRIG
	// start Alpaca on the AP.
	podAp_AlpacaDiscoveryServer = new AlpacaDiscoveryServer(PodWiFi.softAPIP());
	podAp_AlpacaDiscoveryServer->startServer();
	podAp_AlpacaServer = new AlpacaServer(PodWiFi.softAPIP());
	podAp_AlpacaServer->startServer();

	// start Alpaca on the client connection to local wifi
	if(PodWiFi.status() == WL_CONNECTED) {
		pod_AlpacaDiscoveryServer = new AlpacaDiscoveryServer(PodWiFi.localIP());
		pod_AlpacaDiscoveryServer->startServer();
		podAp_AlpacaServer = new AlpacaServer(PodWiFi.localIP());
		podAp_AlpacaServer->startServer();
	}
}

// core 1 main loop takes care of Alpaca coms
void loop()
{
    const TickType_t xDelay = 1 / portTICK_PERIOD_MS;

	podAp_AlpacaDiscoveryServer->checkForRequest();
    podAp_AlpacaServer->checkForRequest();

	if(PodWiFi.status() == WL_CONNECTED) {
		pod_AlpacaDiscoveryServer->checkForRequest();
		pod_AlpacaServer->checkForRequest();
	}
    // FreeRTOS task management
    vTaskDelay(xDelay);
	taskYIELD();
	esp_task_wdt_reset();

}



// core 0  task(s)

void MotorTask(void *)
{
	const TickType_t xDelay = 50/ portTICK_PERIOD_MS; // 50ms task block to give time back
	EncoderConfig motorEncoderConfig;
	globalPodConfig->LoadEncodeConfig(motorEncoderConfig);
	if(motorEncoderConfig.closeAngle <1 && motorEncoderConfig.openAngle <1) {
		motorEncoderConfig.bNeedCalibration = true;
	}

	// create new motor controller
	PodMotorController = new motorMotion();

	for(;;) {
		// run motor if needed
		//
		// FreeRTOS task management
		vTaskDelay(xDelay);
		taskYIELD();
		esp_task_wdt_reset();
	}
}

void PowerTask(void *)
{
	const TickType_t xDelay = 50/ portTICK_PERIOD_MS; // 50ms task block to give time back
	EncoderConfig motorEncoderConfig;
	globalPodConfig->LoadPowerConfig(powerConfig);
	// set all interrupts

	for(;;) {
		// do a whole lot of nothing
		// FreeRTOS task management
		vTaskDelay(xDelay);
		taskYIELD();
		esp_task_wdt_reset();
	}
}

void configureWiFi()
{
	bool bWiFiAPOk = false;

	DBPrintln("========== Configuring WiFi ==========");
	globalPodConfig->LoadIpConfig(wifiClientConfig);
	globalPodConfig->LoadApConfig(wifiApConfig);
	if(wifiApConfig.sSSID.length() < 8) {
		globalPodConfig->setWifiDefault();
	}
	// we run in dual mode AP + Client
	PodWiFi.mode(WIFI_AP_STA);
	// Start AP (aka Hotspot)
	if(wifiApConfig.sSSID.length()) {
		bWiFiAPOk = PodWiFi.softAP(wifiApConfig.sSSID.c_str(), wifiApConfig.sPassword.c_str());
	}

	// connect to local WiFi
	if(wifiClientConfig.sSSID.lemgth()) {
		PodWiFi.begin(sSSID.c_str(), sPassword.c_str());
		while (PodWiFi.status() != WL_CONNECTED) {
			DBPrintln("Waiting for WiFi");
			vTaskDelay(1000 / portTICK_PERIOD_MS);
			nTimeout++;
			if(nTimeout>20) { // 20 seconds should be plenty
				DBPrintln("========== Failed to connect to local wifi ==========");
				break;
			}
		}

	}

	PodWiFi.setHostname("FLOPod");å
	DBPrintln("WiFi IP = " + IpAddress2String(WiFi.softAPIP()));
}
