//
// FLO POD controller
//
// Copyright © 2026 AStarBox. All rights reserved.
//

#include "Arduino.h"
#include <rtc_wdt.h>
#include <esp_task_wdt.h>

#include "config.h"
#include "powerManagement.h"
#include "motorCtrl.h"
#include "podController.h"
#include "environment.h"

String sLocalIPAdress = "";
WIFIConfig	wifiApConfig;
IPConfig	wifiClientConfig;
IPConfig	wifiConfig;
PowerConfig powerConfig;

// include Alpaca here so it gets the definition above.
#include "AlpacaAPI.h"

// interrupt handlers
void magnetHandler();
void overCurrentAlarm();
void mainOverCurrentAlarm();
volatile bool bOcTriggered = false;
volatile bool bMainOcTriggered = false;
volatile bool bMagnetTriggered = false;

// FreeRTOS task
void MotorTask(void *);
void PowerTask(void *);
void EnvTask(void *);

// Environment global variables
float fTemperature;
float fHumidity;
int32_t	rainSensorAdcValue;

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
	// start the network stack so all server sees all interfaces.
	Network.begin();
	// Start local hostspot and connect to local wifi if configured and available
	configureWiFi();
#ifdef USE_ETHERNET
    initEthernet();
#endif

    // start I2C
    Wire.begin();

	// create new motor controller, it will be used by MotorTask and by the PodController
	PodMotorController = new motorCtrl();
	// create new power controller, it will be used by the PodController
	podPowerController = new powerPorts();

    // create tasks
    xTaskCreatePinnedToCore(MotorTask, "MotorTask", 10000, NULL, 8, NULL,  0); // priority 8 (medium) on Core 0
    xTaskCreatePinnedToCore(PowerTask, "PowerTask", 10000, NULL, 16, NULL,  0); // priority 16 (High) on Core 0
    xTaskCreatePinnedToCore(EnvTask, "EnvTask", 10000, NULL, 12, NULL,  0); // priority 12 (between medium and high) on Core 0

	// MAG_TRIG interrupt
	attachInterrupt(MAG_TRIG, magnetHandler, FALLING);

	// create Pod controller PodMotorController config will be set in MotorTask
	podController = new PodController(PodMotorController, podPowerController);

	// start Alpaca on the AP.
/*/
	podAp_AlpacaDiscoveryServer = new AlpacaDiscoveryServer(PodWiFi.softAPIP());
	podAp_AlpacaDiscoveryServer->startServer();
	podAp_AlpacaServer = new AlpacaServer(PodWiFi.softAPIP());
	podAp_AlpacaServer->startServer();

	// start Alpaca on the client connection to local wifi
	if(PodWiFi.status() == WL_CONNECTED) {
*/
		pod_AlpacaDiscoveryServer = new AlpacaDiscoveryServer();
		// pod_AlpacaDiscoveryServer = new AlpacaDiscoveryServer(PodWiFi.localIP());
		pod_AlpacaDiscoveryServer->startServer();
		pod_AlpacaServer = new AlpacaServer();
		// pod_AlpacaServer = new AlpacaServer(PodWiFi.localIP());
		pod_AlpacaServer->startServer();
//	}
}

// core 1 main loop takes care of Alpaca coms
void loop()
{
    const TickType_t xDelay = 1 / portTICK_PERIOD_MS;

	// podAp_AlpacaDiscoveryServer->checkForRequest();
    //podAp_AlpacaServer->checkForRequest();

	//if(PodWiFi.status() == WL_CONNECTED) {
		pod_AlpacaDiscoveryServer->checkForRequest();
		pod_AlpacaServer->checkForRequest();
	//}
    // FreeRTOS task management
    vTaskDelay(xDelay);
	taskYIELD();
	esp_task_wdt_reset();

}


// core 0  task(s)

void MotorTask(void *)
{
	const TickType_t xDelay = 50/ portTICK_PERIOD_MS; // 50ms task block to give time back
	// make sure nothing is moving when we power up
	PodMotorController->Stop();

	for(;;) {
		// check magnet
		if(bMagnetTriggered) {

		}
		// run motor if needed

		// FreeRTOS task management
		vTaskDelay(xDelay);
		taskYIELD();
		esp_task_wdt_reset();
	}
}

void PowerTask(void *)
{
	const TickType_t xDelay = 50/ portTICK_PERIOD_MS; // 50ms task block to give time back
	// set all interrupts
	// OC_ALARM
	attachInterrupt(OC_ALARM, overCurrentAlarm, FALLING);
	// MAIN_OC_ALARM
	attachInterrupt(MAIN_OC_ALARM, mainOverCurrentAlarm, FALLING);
	// read save port config
	globalPodConfig->LoadPowerConfig(powerConfig);
	// set port states
	if(podPowerController) {
		podPowerController->setPortState(DC1, powerConfig.bDc1On);
		podPowerController->setPortState(DC2, powerConfig.bDc2On);
		podPowerController->setPortState(PWM1, powerConfig.nPwm1Percent);
		podPowerController->setPortState(PWM2, powerConfig.nPwm2Percent);
		podPowerController->setPortState(USB_C, powerConfig.bUsbcOn);
	}
	else {

	}
	for(;;) {
		// do a whole lot of nothing
		if(bOcTriggered) {
			bOcTriggered = false;
			// check which INA260 triggered the OC interrupt

		}
		if(bMainOcTriggered){
			bMainOcTriggered = false;
			// check which INA260 triggered the OC interrupt
		}
		// FreeRTOS task management
		vTaskDelay(xDelay);
		taskYIELD();
		esp_task_wdt_reset();
	}
}

void EnvTask(void *)
{
	const TickType_t xDelay = 50/ portTICK_PERIOD_MS; // 50ms task block to give time back

	humTempSensor = new HumTempSensor();
	podRainSensor = new RainSensor();

	for(;;) {
		humTempSensor->getTempAndHum(fTemperature, fHumidity);
		rainSensorAdcValue = podRainSensor->getADCValue();
		// FreeRTOS task management
		vTaskDelay(xDelay);
		taskYIELD();
		esp_task_wdt_reset();
	}
}

// Interrup handlers

void IRAM_ATTR magnetHandler()
{
	// re-read the pin
	if (digitalRead(MAG_TRIG) == LOW) {
		bMagnetTriggered = true;
	}

}

void IRAM_ATTR overCurrentAlarm()
{
	// re-read the pin
	if (digitalRead(OC_ALARM) == LOW) {
		bOcTriggered = true;
	}
}

void IRAM_ATTR mainOverCurrentAlarm()
{
	// re-read the pin
	if (digitalRead(MAIN_OC_ALARM) == LOW) {
		bMainOcTriggered = true;
	}
}


void configureWiFi()
{
	bool bWiFiAPOk = false;
	int nTimeout = 0;

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
	if(wifiClientConfig.sSSID.length()) {
		PodWiFi.begin(wifiClientConfig.sSSID.c_str(), wifiClientConfig.sPassword.c_str());
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

	PodWiFi.setHostname("FLOPod");
	DBPrintln("WiFi IP = " + IpAddress2String(WiFi.softAPIP()));
}

#ifdef USE_ETHERNET
bool initEthernet()
{
	char macBuffer[20];
	bool bDhcpOk = false;
	int nTimeout = 0;

	DBPrintln("========== Init Ethernet ==========");
	// resetChip(ETHERNET_RESET);
	SPI.begin(ETH_SPI_SCK, ETH_SPI_MISO, ETH_SPI_MOSI);
	// network configuration
	if(!ETH.begin(ETH_PHY_TYPE, ETH_PHY_ADDR, ETH_PHY_CS, ETH_PHY_IRQ, ETH_PHY_RST, SPI)) {
		DBPrintln("No Ethernet hardware detected");
		return false;
	}

	// set an ip so we can get the link status
	PodEthernet.config("192.168.0.100", "192.168.0.1", "255.255.255.0");
	while(!PodEthernet.linkUp() ) {
		vTaskDelay(250 / portTICK_PERIOD_MS);
		nTimeout++;
		if(nTimeout == 10) {
			return false;
		}
	}

	PodEthernet.macAddress(MAC_Address);
	PodEthernet.setHostname("FLO-Pod");

	DBPrintln("========== Setting IP config ==========");
    bDhcpOk = PodEthernet.config(IPAddress(0, 0, 0, 0), IPAddress(0, 0, 0, 0), IPAddress(0, 0, 0, 0)); // all value set to the default 0 means use dhcp.
    if(bDhcpOk) {
        nTimeout = 0;
        while(PodEthernet.localIP() == IPAddress(0,0,0,0) ) {
            vTaskDelay(250 / portTICK_PERIOD_MS);
            nTimeout++;
            if(nTimeout == 30) {
                break;
            }
        }
    }

	if(PodEthernet.localIP() == IPAddress(0,0,0,0)) {
			PodEthernet.config("192.168.0.100", "192.168.0.1", "255.255.255.0");
            PodEthernet.dnsIP(0,"1.1.1.1");
            vTaskDelay(250 / portTICK_PERIOD_MS);
	}

	PodEthernet.setDefault();

	DBPrintln("========== Checking hardware status ==========");
	DBPrintln("W5500 Ok.");
	DBPrintln("W5500 IP = " + IpAddress2String(PodEthernet.localIP()));
	snprintf(macBuffer,20,"%02x:%02x:%02x:%02x:%02x:%02x",
		MAC_Address[0],
		MAC_Address[1],
		MAC_Address[2],
		MAC_Address[3],
		MAC_Address[4],
		MAC_Address[5]);
	DBPrintln("Dome MAC : " + String(macBuffer));
	return true;
}
#endif
