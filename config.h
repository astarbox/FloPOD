//
// Pulsar Imaging Pod
// Firmware configuration definition and management
// Copyright © 2026 AStarBox. All rights reserved.
//
#ifndef __P_CONFIG__
#define __P_CONFIG__
#include <Network.h>
#include <Wire.h>
#include <Preferences.h>
#include <nvs_flash.h>
#include <Network.h>
#include <WiFi.h>
#include <esp32-hal-ledc.h>

#define PodWiFi WiFi

#define USE_ETHERNET

#define DEBUG   // enable debug to serial port defined as DebugPort

// temp & hum sensor
// define one OR the other, not both..
// #define USE_HDC1080
#define USE_SHT30

// #define USE_FDC1004
#define USE_FDC2112

#ifdef DEBUG
#pragma message "Debug messages enabled"
#define DebugPort Serial
#define DBPrint(x) if(DebugPort) DebugPort.print(x)
#define DBPrintln(x) if(DebugPort) DebugPort.println(x)
#define DBPrintHex(x) if(DebugPort) DebugPort.print(x, HEX)
#else
#pragma message "Debug messages disabled"
#define DBPrint(x)
#define DBPrintln(x)
#define DBPrintHex(x)
#endif // DEBUG


#ifdef USE_ETHERNET
#pragma message "Ethernet enabled"
#include <ETH.h>
// network interfaces
#define PodEthernet ETH

#define ETHERNET_CS     5
#define ETHERNET_RESET  46

#define ETH_PHY_TYPE ETH_PHY_W5500
#define ETH_PHY_ADDR 1
#define ETH_PHY_CS   ETHERNET_CS
#define ETH_PHY_IRQ  -1
#define ETH_PHY_RST  ETHERNET_RESET

// SPI pins
#define ETH_SPI_SCK         SCK
#define ETH_SPI_MISO        MISO
#define ETH_SPI_MOSI        MOSI

byte MAC_Address[6];
#endif // USE_ETHERNET


#define VERSION "0.1"
String podHostname;

// input
#define MAG_TRIG        2
#define OC_ALARM        3
#define MAIN_OC_ALARM   20

// ouput
#define MOTOR_V_EN		7
#define HEATER          15
#define USB_C           16
#define BAT_EN          17
#define LED1            18
#define LED2            19
#define DC1             35
#define DC2             36
#define PWM1            37
#define PWM2            38

// Motors
#define MOT1_EN          39  // MOT1_1 , Motor PWM
#define MOT1_PH          40  // MOT1_2 , Direction
#define MOT1_SLEEP       41  // MOT1_3 , sleep
#define MOT1_FAULT       42  // MOT1_4 , fault, set to input

#define MOT2_EN          4  // MOT2_1 , Motor PWM
#define MOT2_PH          5  // MOT2_2 , Direction
#define MOT2_SLEEP       6  // MOT2_3 , sleep
#define MOT2_FAULT       7  // MOT2_4 , fault, set to input

// PWM port settingz
#define PWM_FREQ 			20000
#define LEDC_TIMER_12_BIT	12

// over current alarm default threshold
#define DEFAULT_MAIN_OC	10.0
#define DEFAULT_DC_OC	 3.5
#define DEFAULT_PWM_OC	 3.5
#define DEFAULT_USBC_OC	 3.5


const int MAX_DUTY_CYCLE = (int)(pow(2, LEDC_TIMER_12_BIT) - 1);

const int Motor1PwmChannel = 0;
const int Motor2PwmChannel = 1;
const int PWM1PwmChannel = 2;
const int PWM2PwmChannel = 3;


// config for station mode
typedef struct IPCONFIG {
	bool		bUseDHCP;
	IPAddress	ip;
	IPAddress	dns;
	IPAddress	gateway;
	IPAddress	netmask;
} IPConfig;

typedef struct WIFICONFIG {
	String	sSSID;
	String	sPassword;
	int		nChannel;
} WIFIConfig;

// other config
typedef struct PodConfiguration {
	float			openPos;
	float			closedPos;
	byte			serialNum[6];
	int				podType;
} Configuration;

//power port config
typedef struct POWERCONFIG {
	bool	bDc1On;
	bool	bDc2On;
	int		nPwm1Percent;
	int		nPwm2Percent;
	bool	bUsbcOn;
	float	fMain_OC;
	float	fDc1_OC;
	float	fDc2_OC;
	float	fPwm1_OC;
	float	fPwm2_OC;
	float	fUsbc_OC;
} PowerConfig;

// rotation encode config
typedef struct ENCODER_CONFIG {
	float	closeAngle;
	float	openAngle;
	bool	bIsCalibrated;
} EncoderConfig;

enum podStates { OPEN, CLOSED, IDLE, OPENING, CLOSING, FINISHING_OPENING, FINISHING_CLOSING, CALIBRATION_STEP_RESET, CALIBRATION_STEP_OPENING, CALIBRATION_STEP_OPEN, CALIBRATION_MEASURE, POD_ERROR};
enum podType {POD_60=0, POD_100, POD_BIG, POD_NEXT_BIG};

String IpAddress2String(const IPAddress& ipAddress)
{
  return String(ipAddress[0]) + String(".") +
  		String(ipAddress[1]) + String(".") +
		String(ipAddress[2]) + String(".") +
		String(ipAddress[3]);
}

class PodConfig
{
public:
	PodConfig();

	void LoadIpConfig(IPConfig &ipClientConfig);
	void saveIpConfig(IPConfig ipClientConfig);

	void LoadApConfig(WIFIConfig &wifiApConfig);
	void saveApConfig(WIFIConfig wifiApConfig);

	void LoadStaConfig(WIFIConfig &wifiStaConfig);
	void saveStaConfig(WIFIConfig wifiStaConfig);

	void LoadPodConfig(Configuration &podConfig);
	void savePodConfig(Configuration podConfig);

	void LoadPowerConfig(PowerConfig &powerConfig);
	void savePowerConfig(PowerConfig powerConfig);

	void LoadEncoderConfig(EncoderConfig &encoderConfig);
	void saveEncoderConfig(EncoderConfig encoderConfig);

	void setWifiDefault();
	void getSerialNumber(String &serNum);
	void getSerialNumberRaw(byte *serNum);

	void setAlpacaPortName(int nPort, String sName);
	void getAlpacaPortName(int nPort, String &sName);
	void resetAllSettings();

private:
	Preferences m_preferences;
};

PodConfig *globalPodConfig; // init GPIO, provide config management


PodConfig::PodConfig()
{
	bool nvsInitDone = false;

	DBPrintln("PodConfig::PodConfig");
	m_preferences.begin("PodConfig", false);
	nvsInitDone = m_preferences.isKey("nvsInit");
	if(!nvsInitDone) {
		DBPrintln("Initializing NVS");
		m_preferences.end();
		nvs_flash_erase();
		nvs_flash_init();
		m_preferences.begin("PodConfig", false);
		m_preferences.putBool("nvsInit", true);

	}
	m_preferences.end();

	// set pwm clock source
	ledcSetClockSource(LEDC_AUTO_CLK);

	// configure input pins
	pinMode(MAG_TRIG,           INPUT_PULLUP);
	pinMode(OC_ALARM,           INPUT_PULLUP);
	pinMode(MAIN_OC_ALARM,      INPUT_PULLUP);
	pinMode(MOT1_FAULT,          INPUT_PULLUP);
	pinMode(MOT2_FAULT,          INPUT_PULLUP);

	// output
	pinMode(HEATER,     OUTPUT);
	pinMode(USB_C,      OUTPUT);
	pinMode(BAT_EN,     OUTPUT);
	pinMode(LED1,       OUTPUT);
	pinMode(LED2,       OUTPUT);
	pinMode(DC1,        OUTPUT);
	pinMode(DC2,        OUTPUT);
	pinMode(LED2,       OUTPUT);
	pinMode(PWM1,       OUTPUT);
	pinMode(PWM2,       OUTPUT);
	pinMode(MOT1_EN,     OUTPUT); // MOT1-1 , Motor PWM
	pinMode(MOT1_PH,     OUTPUT); // MOT1-2 , Direction
	pinMode(MOT1_SLEEP,  OUTPUT); // MOT1-3
	pinMode(MOT2_EN,     OUTPUT); // MOT2-1 , Motor PWM
	pinMode(MOT2_PH,     OUTPUT); // MOT2-2 , Direction
	pinMode(MOT2_SLEEP,  OUTPUT); // MOT2-3

}

void PodConfig::LoadIpConfig(IPConfig &ipClientConfig)
{
	m_preferences.begin("PodConfig", false);
	ipClientConfig.bUseDHCP = m_preferences.getBool("clientUseDhcp",false);
	if(!ipClientConfig.bUseDHCP ) {
		// load configured static IP
		ipClientConfig.ip.fromString( m_preferences.getString("clientIP","172.16.42.100"));
		ipClientConfig.netmask.fromString( m_preferences.getString("netmask","255.255.255.0"));
		ipClientConfig.gateway.fromString( m_preferences.getString("clientGateway","172.16.42.1"));
		ipClientConfig.dns.fromString( m_preferences.getString("clientDNS","1.1.1.1"));
	}
	m_preferences.end();
}

void PodConfig::saveIpConfig(IPConfig ipClientConfig)
{
	m_preferences.begin("PodConfig", false);
	m_preferences.putBool("clientUseDhcp", ipClientConfig.bUseDHCP);
	if(!ipClientConfig.bUseDHCP ) {
		// load configured static IP
		m_preferences.putString("clientIP",IpAddress2String(ipClientConfig.ip));
		m_preferences.putString("netmask",IpAddress2String(ipClientConfig.netmask));
		m_preferences.putString("clientGateway",IpAddress2String(ipClientConfig.gateway));
		m_preferences.putString("clientDNS",IpAddress2String(ipClientConfig.dns));
	}
	m_preferences.end();
}

void PodConfig::LoadApConfig(WIFIConfig &wifiApConfig)
{
	m_preferences.begin("PodConfig", false);
	wifiApConfig.sSSID =  m_preferences.getString("APSSID","PulsarPod");
	wifiApConfig.sPassword =  m_preferences.getString("APPassword","PulsarPod");
	wifiApConfig.nChannel = m_preferences.getInt("APChannel", 6);
	m_preferences.end();
}

void PodConfig::saveApConfig(WIFIConfig wifiApConfig)
{
	m_preferences.begin("PodConfig", false);
	m_preferences.putString("APSSID", wifiApConfig.sSSID);
	m_preferences.putString("APPassword",wifiApConfig.sPassword);
	m_preferences.putInt("APChannel",wifiApConfig.nChannel);
	m_preferences.end();
}

void PodConfig::LoadStaConfig(WIFIConfig &wifiStaConfig)
{
	m_preferences.begin("PodConfig", false);
	wifiStaConfig.sSSID =  m_preferences.getString("StaSSID","NOT_CONFIGURED");
	wifiStaConfig.sPassword =  m_preferences.getString("StaPassword","");
	m_preferences.end();
}

void PodConfig::saveStaConfig(WIFIConfig wifiStaConfig)
{
	m_preferences.begin("PodConfig", false);
	m_preferences.putString("StaSSID", wifiStaConfig.sSSID);
	m_preferences.putString("StaPassword",wifiStaConfig.sPassword);
	m_preferences.end();
}

void PodConfig::LoadPodConfig(Configuration &podConfig)
{
	m_preferences.begin("PodConfig", false);
	podConfig.openPos = m_preferences.getFloat("openPos", 0);
	podConfig.closedPos = m_preferences.getFloat("closedPos", 90);
	podConfig.podType = m_preferences.getInt("podType", POD_60);
	getSerialNumberRaw(podConfig.serialNum);
	m_preferences.end();

}

void PodConfig::savePodConfig(Configuration podConfig)
{
	m_preferences.begin("PodConfig", false);
	m_preferences.putFloat("openPos", podConfig.openPos);
	m_preferences.putFloat("closedPos", podConfig.closedPos);
	m_preferences.putInt("podType", podConfig.podType);
	m_preferences.end();
}


void PodConfig::LoadPowerConfig(PowerConfig &powerConfig)
{
	m_preferences.begin("PodConfig", false);
	powerConfig.bDc1On = m_preferences.getBool("DC1_State", false);
	powerConfig.bDc2On = m_preferences.getBool("DC1_State", false);
	powerConfig.bUsbcOn = m_preferences.getBool("USBC_State", false);
	powerConfig.nPwm1Percent = m_preferences.getInt("Pwm1Percent", 0);
	powerConfig.nPwm2Percent = m_preferences.getInt("Pwm2Percent", 0);
	powerConfig.fMain_OC = m_preferences.getFloat("Main_OC", DEFAULT_MAIN_OC);
	powerConfig.fDc1_OC = m_preferences.getFloat("Dc1_OC", DEFAULT_DC_OC);
	powerConfig.fDc2_OC = m_preferences.getFloat("Dc2_OC", DEFAULT_DC_OC);
	powerConfig.fPwm1_OC = m_preferences.getFloat("Pwm1_OC", DEFAULT_PWM_OC);
	powerConfig.fPwm2_OC = m_preferences.getFloat("Pwm2_OC", DEFAULT_PWM_OC);
	powerConfig.fUsbc_OC = m_preferences.getFloat("Usbc_OC", DEFAULT_USBC_OC);
	m_preferences.end();
}

void PodConfig::savePowerConfig(PowerConfig powerConfig)
{
	m_preferences.begin("PodConfig", false);
	m_preferences.putBool("DC1_State", powerConfig.bDc1On);
	m_preferences.putBool("DC1_State", powerConfig.bDc2On);
	m_preferences.putBool("USBC_State", powerConfig.bUsbcOn);
	m_preferences.putInt("Pwm1Percent", powerConfig.nPwm1Percent);
	m_preferences.putInt("Pwm2Percent", powerConfig.nPwm2Percent);
	m_preferences.putFloat("Main_OC", powerConfig.fMain_OC);
	m_preferences.putFloat("Dc1_OC", powerConfig.fDc1_OC);
	m_preferences.putFloat("Dc2_OC", powerConfig.fDc2_OC);
	m_preferences.putFloat("Pwm1_OC", powerConfig.fPwm1_OC);
	m_preferences.putFloat("Pwm2_OC", powerConfig.fPwm2_OC);
	m_preferences.putFloat("Usbc_OC", powerConfig.fUsbc_OC);
	m_preferences.end();
}

void PodConfig::LoadEncoderConfig(EncoderConfig &encoderConfig)
{
	m_preferences.begin("PodConfig", false);
	encoderConfig.bIsCalibrated = m_preferences.getBool("isCalibrated", false);
	encoderConfig.closeAngle = m_preferences.getFloat("closeAngle",0);
	encoderConfig.openAngle = m_preferences.getFloat("openAngle",0);

	m_preferences.end();
}

void PodConfig::saveEncoderConfig(EncoderConfig encoderConfig)
{
	m_preferences.begin("PodConfig", false);
	m_preferences.putBool("isCalibrated", encoderConfig.bIsCalibrated);
	m_preferences.putFloat("closeAngle",encoderConfig.closeAngle);
	m_preferences.putFloat("openAngle",encoderConfig.openAngle);

	m_preferences.end();
}

void PodConfig::setWifiDefault()
{
	m_preferences.begin("PodConfig", false);
	// AP default
	m_preferences.putString("APSSID", "PulsarPod");
	m_preferences.putString("APPassword","PulsarPod");
	m_preferences.putInt("APChannel",6);
	// No STA SSID/password
	m_preferences.remove("StaSSID");
	m_preferences.remove("StaPassword");
	m_preferences.end();
}

void PodConfig::getSerialNumber(String &serNum)
{
	uint64_t nFuseMac = ESP.getEfuseMac();
	byte nSerNum[7];
	getSerialNumberRaw(nSerNum);
	nSerNum[6] = 0x00;
	serNum = String(nSerNum, HEX);
	DBPrintln("Serial : " + String(nSerNum, HEX));
}

void PodConfig::getSerialNumberRaw(byte *nSerNum)
{
	uint64_t nFuseMac = ESP.getEfuseMac();
	nSerNum[0] = (byte)(nFuseMac>>48);
	nSerNum[1] = (byte)(nFuseMac>>32);
	nSerNum[2] = (byte)(nFuseMac>>24);
	nSerNum[3] = (byte)(nFuseMac>>16);
	nSerNum[4] = (byte)(nFuseMac>>8);
	nSerNum[5] = (byte)(nFuseMac);
}

void PodConfig::setAlpacaPortName(int nPort, String sName)
{
	String sPort;

	switch(nPort) {
		case DC1:
			sPort="DC1_name";
			break;
		case DC2:
			sPort="DC2_name";
			break;
		case PWM1:
			sPort="PWM11_name";
			break;
		case PWM2:
			sPort="PWM2_name";
			break;
		case USB_C:
			sPort="USB_C_name";
			break;
	}
	m_preferences.begin("PodConfig", false);
	m_preferences.putString(sPort.c_str(), sName);
	m_preferences.end();
}

void PodConfig::getAlpacaPortName(int nPort, String &sName)
{
	String sPort;
	String sDefaultName;
	switch(nPort) {
		case DC1:
			sPort="DC1_name";
			sDefaultName="DC1";
			break;
		case DC2:
			sPort="DC2_name";
			sDefaultName="DC2";
			break;
		case PWM1:
			sPort="PWM11_name";
			sDefaultName="PWM1";
			break;
		case PWM2:
			sPort="PWM2_name";
			sDefaultName="PWM1";
			break;
		case USB_C:
			sPort="USB_C_name";
			sDefaultName="USB-C";
			break;
	}
	m_preferences.begin("PodConfig", false);
	sName = m_preferences.getString(sPort.c_str(), sDefaultName);
	m_preferences.end();

}


void PodConfig::resetAllSettings()
{
	// save device type
	int nCurrentPodType;

	m_preferences.begin("PodConfig", false);
	nCurrentPodType = m_preferences.getInt("podType", 1);
	m_preferences.end();

	nvs_flash_erase();
	nvs_flash_init();
	m_preferences.begin("PodConfig", false);
	m_preferences.putBool("nvsInit", true);
	// set device type back
	m_preferences.putInt("podType", nCurrentPodType);
	m_preferences.end();
	ESP.restart();
}



#endif
