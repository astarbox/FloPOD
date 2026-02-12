//
// FLO POD controller
// Firmware configuration definition and management
// Copyright © 2026 AStarBox. All rights reserved.
//
#ifndef __P_CONFIG__
#define __P_CONFIG__
#include <Preferences.h>
#include <nvs_flash.h>
#include <WiFi.h>

#define PodWiFi WiFi

#define DEBUG   // enable debug to serial port defined as DebugPort

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


#define VERSION "0.1"

// input
#define MOTOR_CURRENT   1 // Analog input
#define MAG_TRIG        2
#define OC_ALARM        3
#define MAIN_OC_ALARM   20

// ouput
#define HEATER          15
#define USB_C           16
#define BAT_EN          17
#define LED1            18
#define LED2            19
#define DC1             35
#define DC2             36
#define PWM1            37
#define PWM2            38
#define MOT_EN          39  // MOT1 , Motor PWM
#define MOT_PH          40  // MOT2 , Direction
#define MOT_SLEEP       41  // MOT3
#define MOT_FAULT       42  // MOT4


// config for station mode
typedef struct IPCONFIG {
	String          sSSID;
	String          sPassword;
    bool            bUseDHCP;
	IPAddress       ip;
	IPAddress       dns;
	IPAddress       gateway;
	IPAddress       netmask;
} IPConfig;

typedef struct WIFICONFIG {
	String sSSID;
	String sPassword;
} WIFIConfig;

// other config
typedef struct PodConfiguration {
    float			openPos;
    float			closedPos;
	byte			serialNum[6];
} Configuration;

//power port config
typedef struct POWERCONFIG {
    bool    bDc1On;
    bool    bDc2On;
    int     nPwm1Percent;
    int     nPwm2Percent;
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
    float   closeAngle;
    float   openAngle;
	bool	bNeedCalibration;
} EncoderConfig;

enum podStates { OPEN, CLOSED, IDLE, OPENING, CLOSING, FINISHING_OPENING, FINISHING_CLOSING, CALIBRATION_STEP_RESET, CALIBRATION_STEP_OPENING, CALIBRATION_STEP_OPEN, CALIBRATION_MEASURE, POD_ERROR};

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
    void LoadApConfig(WIFIConfig &wifiApConfig);
    void LoadPodConfig(Configuration &podConfig);
    void LoadPowerConfig(PowerConfig &powerConfig);
    void LoadEncodeConfig(EncoderConfig &encoderConfig);

	void setWifiDefault();
	void getSerialNumber(String &serNum);

private:
    Preferences m_preferences;
};

PodConfig::PodConfig()
{
    bool nvsInitDone = false;

    DBPrintln("PodConfig::PodConfig");
	m_preferences.begin("FloPod", false);
	nvsInitDone = m_preferences.isKey("nvsInit");
	if(!nvsInitDone) {
		DBPrintln("Initializing NVS");
		m_preferences.end();
		nvs_flash_erase();
		nvs_flash_init();
		m_preferences.begin("FloPod", false);
		m_preferences.putBool("nvsInit", true);

	}
    m_preferences.end();

    // configure input pins
	pinMode(MOTOR_CURRENT,      INPUT_PULLUP);
	pinMode(MAG_TRIG,           INPUT_PULLUP);
	pinMode(OC_ALARM,           INPUT_PULLUP);
	pinMode(MAIN_OC_ALARM,      INPUT_PULLUP);
	pinMode(MOT_FAULT,          INPUT_PULLUP);

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
	pinMode(MOT_EN,     OUTPUT); // MOT1 , Motor PWM
	pinMode(MOT_PH,     OUTPUT); // MOT2 , Direction
	pinMode(MOT_SLEEP,  OUTPUT); // MOT3

}

void PodConfig::LoadIpConfig(IPConfig &ipClientConfig)
{
	m_preferences.begin("FloPod", false);
	ipClientConfig.bUseDHCP = m_preferences.getBool("clientUseDhcp",false);
	ipClientConfig.sSSID = m_preferences.getString("clientSSID","");
	ipClientConfig.sPassword = m_preferences.getString("clientSSID","");
	if(!ipClientConfig.bUseDHCP ) {
		// load configured static IP
		ipClientConfig.ip.fromString( m_preferences.getString("clientIP","169.254.254.123"));
		ipClientConfig.netmask.fromString( m_preferences.getString("netmask","255.255.255.0å"));
		ipClientConfig.gateway.fromString( m_preferences.getString("clientGateway","169.254.254.1"));
		ipClientConfig.dns.fromString( m_preferences.getString("clientDNS","1.1.1.1"));
	}
	m_preferences.end();
}

void PodConfig::LoadApConfig(WIFIConfig &wifiApConfig)
{
	m_preferences.begin("FloPod", false);
	wifiApConfig.sSSID =  m_preferences.getString("APSSID","FLO_Pod");
	wifiApConfig.sPassword =  m_preferences.getString("APPassword","FLO_Pod");
	m_preferences.end();
}

void PodConfig::LoadPodConfig(Configuration &podConfig)
{
	m_preferences.begin("FloPod", false);
	m_preferences.end();

}

void PodConfig::LoadPowerConfig(PowerConfig &powerConfig)
{
	m_preferences.begin("FloPod", false);
	m_preferences.end();

}

void PodConfig::LoadEncodeConfig(EncoderConfig &encoderConfig)
{
	m_preferences.begin("FloPod", false);
	encoderConfig.bNeedCalibration = m_preferences.getBool("bNeedCalibration", false);
	encoderConfig.closeAngle = m_preferences.getFloat("closeAngle",0);
	encoderConfig.openAngle = m_preferences.getFloat("openAngle",0);

	m_preferences.end();

}

void PodConfig::setWifiDefault()
{
	m_preferences.begin("FloPod", false);
	m_preferences.end();
}

void PodConfig::getSerialNumber(String &serNum)
{
	uint64_t nFuseMac = ESP.getEfuseMac();
	byte nSerNum[7];
	nSerNum[0] = (byte)(nFuseMac>>48);
	nSerNum[1] = (byte)(nFuseMac>>32);
	nSerNum[2] = (byte)(nFuseMac>>24);
	nSerNum[3] = (byte)(nFuseMac>>16);
	nSerNum[4] = (byte)(nFuseMac>>8);
	nSerNum[5] = (byte)(nFuseMac);
	nSerNum[7] = 0x00;
	serNum = String(nSerNum, HEX);
	DBPrintln("Serial : " + String(nSerNum, HEX));
}

#endif
