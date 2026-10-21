// ImagingPod API functions
//
//  Created by Rodolphe Pineau on 2026/10/19
//
#ifndef __POD_API__
#define __POD_API__
#pragma message "API Rest server enabled"
#include <esp_wifi.h>
#include <vector>
#include <functional>
#include <ArduinoJson.h>
// Alpaca REST server
#include <UUID.h>
#include <aWOT.h>

#include "ApiCommon.h"
#include "podController.h"

#define POD_API_PORT 80

class PodAPIServer
{
public :
	PodAPIServer(int port=POD_API_PORT);
	void startServer();
	void checkForRequest();
private :
	NetworkServer *mRestServer;
	Application  *m_ApiRestServer;
	int m_nRestPort;
	UUID m_ApiServerUUID;
};

PodAPIServer *pod_ApiServer;

PodAPIServer::PodAPIServer(int port)
{
	String sSerialNumber;    // Mac address, uses part of the unique ID

	m_nRestPort = port;
	mRestServer = nullptr;
	m_ApiRestServer = nullptr;

	globalPodConfig->getSerialNumber(sSerialNumber);

	m_ApiServerUUID.seed(sSerialNumber[4],sSerialNumber[5]);
	m_ApiServerUUID.generate();
}

//
// controller settings API
//


void getSerialNumber(Request &req, Response &res)
{
	JsonDocument controllerResp;
	String sResp;
	String sSerialNumber;

	globalPodConfig->getSerialNumber(sSerialNumber);

	controllerResp["value"] = sSerialNumber;
	serializeJson(controllerResp, sResp);
	DBPrintln("sResp : " + sResp);

	res.set("Content-Type", "application/json");
	res.write((uint8_t*)(sResp.c_str()),sResp.length());
}


void useDHCPState(Request &req, Response &res)
{
	JsonDocument controllerResp;
	String sResp;
	bool bUseDhcp = false;

	if(req.method() == Request::PUT) {
		JsonDocument FormData;
		formDataToJson(req, FormData);
		if(FormData.size()==0){
			RestError_x401(controllerResp, res);
			return;
		}
		else {
			if(FormData["value"].is<bool>()) {
				bUseDhcp = FormData["value"];
				// globalPodConfig->setDHCPFlag(bUseDhcp);
			}
		}
	}

	// controllerResp["value"] = globalPodConfig->getDHCPFlag();
	serializeJson(controllerResp, sResp);
	DBPrintln("sResp : " + sResp);

	res.set("Content-Type", "application/json");
	res.write((uint8_t*)(sResp.c_str()),sResp.length());
}

void ipAddressValue(Request &req, Response &res)
{
	JsonDocument controllerResp;
	String sResp;

	if(req.method() == Request::PUT) {
		JsonDocument FormData;
		formDataToJson(req, FormData);
		if(FormData.size()==0){
			RestError_x401(controllerResp, res);
			return;
		}
		else {
			if(FormData["value"].is<String>()) {
				// globalPodConfig->setIPAddress(FormData["value"]);
			}
		}
	}

	// controllerResp["value"] = String(RotatorClass::IpAddress2String(domeEthernet.localIP()));
	serializeJson(controllerResp, sResp);
	DBPrintln("sResp : " + sResp);

	res.set("Content-Type", "application/json");
	res.write((uint8_t*)(sResp.c_str()),sResp.length());
}

void subnetMaskValue(Request &req, Response &res)
{
	JsonDocument controllerResp;
	String sResp;

	if(req.method() == Request::PUT) {
		JsonDocument FormData;
		formDataToJson(req, FormData);
		if(FormData.size()==0){
			RestError_x401(controllerResp, res);
			return;
		}
		else {
			if(FormData["value"].is<String>()) {
				// globalPodConfig->setIPSubnetMask(FormData["value"]);
			}
		}
	}

	// controllerResp["value"] = String(RotatorClass::IpAddress2String(domeEthernet.subnetMask()));
	serializeJson(controllerResp, sResp);
	DBPrintln("sResp : " + sResp);

	res.set("Content-Type", "application/json");
	res.write((uint8_t*)(sResp.c_str()),sResp.length());
}

void ipGatewayValue(Request &req, Response &res)
{
	JsonDocument controllerResp;
	String sResp;

	if(req.method() == Request::PUT) {
		JsonDocument FormData;
		formDataToJson(req, FormData);
		if(FormData.size()==0){
			RestError_x401(controllerResp, res);
			return;
		}
		else {
			if(FormData["value"].is<String>()) {
				// globalPodConfig->setIPGateway(FormData["value"]);
			}
		}
	}

	// controllerResp["value"] = String(RotatorClass::IpAddress2String(domeEthernet.gatewayIP()));
	serializeJson(controllerResp, sResp);
	DBPrintln("sResp : " + sResp);

	res.set("Content-Type", "application/json");
	res.write((uint8_t*)(sResp.c_str()),sResp.length());
}

void podCalibrate(Request &req, Response &res)
{
	JsonDocument controllerResp;
	String sResp;
	MotorCalibrationSteps nCalState;
	if(req.method() == Request::PUT) {
		JsonDocument FormData;
		formDataToJson(req, FormData);
		if(FormData.size()==0){
			RestError_x401(controllerResp, res);
			return;
		}
		else {
			if(FormData["value"].is<String>()) {
				if(FormData["value"] == "start") {
					PodMotorController->Calibrate();
				}
				if(FormData["value"] == "stop") {
					PodMotorController->Stop();
				}
			}
		}
	}
	PodMotorController->getCalState(nCalState);
	controllerResp["value"] = nCalState;
	serializeJson(controllerResp, sResp);
	DBPrintln("sResp : " + sResp);

	res.set("Content-Type", "application/json");
	res.write((uint8_t*)(sResp.c_str()),sResp.length());
}

void environmentData(Request &req, Response &res)
{
	JsonDocument controllerResp;
	String sResp;
	float temp, hum;

	if(humTempSensor && podRainSensor) {
		humTempSensor->getTempAndHum(temp, hum);
		controllerResp["humidity"] = hum;
		controllerResp["temp"] = temp;
		controllerResp["rain"] = podRainSensor->isRaining();
	}
	else {
		RestError_x401(controllerResp, res, "POD can't read environment sensor.");
		return;
	}

	serializeJson(controllerResp, sResp);
	DBPrintln("sResp : " + sResp);

	res.set("Content-Type", "application/json");
	res.write((uint8_t*)(sResp.c_str()),sResp.length());
}

void podOpen(Request &req, Response &res)
{
	JsonDocument controllerResp;
	String sResp;

	PodMotorController->Open();
	controllerResp["value"] = A_OPENING;
	serializeJson(controllerResp, sResp);
	DBPrintln("sResp : " + sResp);

	res.set("Content-Type", "application/json");
	res.write((uint8_t*)(sResp.c_str()),sResp.length());
}

void podClose(Request &req, Response &res)
{
	JsonDocument controllerResp;
	String sResp;
	float fParkAz;

	PodMotorController->Close();
	controllerResp["value"] = A_CLOSING;
	serializeJson(controllerResp, sResp);
	DBPrintln("sResp : " + sResp);

	res.set("Content-Type", "application/json");
	res.write((uint8_t*)(sResp.c_str()),sResp.length());
}

void podState(Request &req, Response &res)
{
	JsonDocument controllerResp;
	String sResp;
	PodShutterState nState;

	PodMotorController->getShutterState(nState);
	switch(nState){
		case PS_OPEN :
			controllerResp["value"] = A_OPEN;
			break;
		case PS_CLOSED :
			controllerResp["value"] = A_CLOSED;
			break;
		case PS_OPENING :
			controllerResp["value"] = A_OPENING;
			break;
		case PS_CLOSING :
			controllerResp["value"] = A_CLOSING;
			break;
		case PS_UNKNOWN :
			controllerResp["value"] = A_ERROR;
			break;
		default:
			controllerResp["value"] = A_ERROR;
			break;
	}
	serializeJson(controllerResp, sResp);
	DBPrintln("sResp : " + sResp);

	res.set("Content-Type", "application/json");
	res.write((uint8_t*)(sResp.c_str()),sResp.length());
}


void podDC1(Request &req, Response &res)
{
	JsonDocument controllerResp;
	String sResp;
	bool bPortOn = false;

	if(req.method() == Request::PUT) {
		JsonDocument FormData;
		formDataToJson(req, FormData);
		if(FormData.size()==0){
			RestError_x401(controllerResp, res);
			return;
		}
		else {
			if(FormData["value"].is<bool>()) {
				bPortOn = FormData["value"];
				if(podPowerController)
					podPowerController->setPortState(DC1, bPortOn);
			}
		}
	}
	podPowerController->getPortState(DC1, bPortOn);
	controllerResp["value"] = bPortOn;
	serializeJson(controllerResp, sResp);
	DBPrintln("sResp : " + sResp);

	res.set("Content-Type", "application/json");
	res.write((uint8_t*)(sResp.c_str()),sResp.length());
}

void podDC2(Request &req, Response &res)
{
	JsonDocument controllerResp;
	String sResp;
	bool bPortOn = false;

	if(req.method() == Request::PUT) {
		JsonDocument FormData;
		formDataToJson(req, FormData);
		if(FormData.size()==0){
			RestError_x401(controllerResp, res);
			return;
		}
		else {
			if(FormData["value"].is<bool>()) {
				bPortOn = FormData["value"];
				if(podPowerController)
					podPowerController->setPortState(DC2, bPortOn);
			}
		}
	}
	podPowerController->getPortState(DC2, bPortOn);
	controllerResp["value"] = bPortOn;
	serializeJson(controllerResp, sResp);
	DBPrintln("sResp : " + sResp);

	res.set("Content-Type", "application/json");
	res.write((uint8_t*)(sResp.c_str()),sResp.length());
}

void podPWM1(Request &req, Response &res)
{
	JsonDocument controllerResp;
	String sResp;
	int nPercent;

	if(req.method() == Request::PUT) {
		JsonDocument FormData;
		formDataToJson(req, FormData);
		if(FormData.size()==0){
			RestError_x401(controllerResp, res);
			return;
		}
		else {
			if(FormData["value"].is<int>()) {
				nPercent = FormData["value"];
				if(podPowerController)
					podPowerController->setPwmPortState(PWM1, nPercent);

			}
		}
	}
	podPowerController->getPwmPortState(PWM1, nPercent);
	controllerResp["value"] = nPercent;
	serializeJson(controllerResp, sResp);
	DBPrintln("sResp : " + sResp);

	res.set("Content-Type", "application/json");
	res.write((uint8_t*)(sResp.c_str()),sResp.length());
}

void podPWM2(Request &req, Response &res)
{
	JsonDocument controllerResp;
	String sResp;
	int nPercent;

	if(req.method() == Request::PUT) {
		JsonDocument FormData;
		formDataToJson(req, FormData);
		if(FormData.size()==0){
			RestError_x401(controllerResp, res);
			return;
		}
		else {
			if(FormData["value"].is<int>()) {
				nPercent = FormData["value"];
				if(podPowerController)
					podPowerController->setPwmPortState(PWM2, nPercent);

			}
		}
	}
	podPowerController->getPwmPortState(PWM2, nPercent);
	controllerResp["value"] = nPercent;
	serializeJson(controllerResp, sResp);
	DBPrintln("sResp : " + sResp);

	res.set("Content-Type", "application/json");
	res.write((uint8_t*)(sResp.c_str()),sResp.length());
}

void podUsbC(Request &req, Response &res)
{
	JsonDocument controllerResp;
	String sResp;
	bool bPortOn = false;

	if(req.method() == Request::PUT) {
		JsonDocument FormData;
		formDataToJson(req, FormData);
		if(FormData.size()==0){
			RestError_x401(controllerResp, res);
			return;
		}
		else {
			if(FormData["value"].is<bool>()) {
				bPortOn = FormData["value"];
				if(podPowerController)
					podPowerController->setPortState(USB_C, bPortOn);
			}
		}
	}
	podPowerController->getPortState(USB_C, bPortOn);
	controllerResp["value"] = bPortOn;
	serializeJson(controllerResp, sResp);
	DBPrintln("sResp : " + sResp);

	res.set("Content-Type", "application/json");
	res.write((uint8_t*)(sResp.c_str()),sResp.length());
}

void podMainPower(Request &req, Response &res)
{
	JsonDocument controllerResp;
	String sResp;
	int nPercent;
	float fValue;

	fValue = podPowerController->readVolts(INA260_MAIN);
	controllerResp["volts"] = fValue;
	fValue = podPowerController->readAmps(INA260_MAIN);
	controllerResp["amps"] = fValue;
	fValue = podPowerController->readPower(INA260_MAIN);
	controllerResp["watt"] = fValue;

	serializeJson(controllerResp, sResp);
	DBPrintln("sResp : " + sResp);

	res.set("Content-Type", "application/json");
	res.write((uint8_t*)(sResp.c_str()),sResp.length());
}

void podDC1Power(Request &req, Response &res)
{
	JsonDocument controllerResp;
	String sResp;
	int nPercent;
	float fValue;

	fValue = podPowerController->readVolts(INA260_DC_1);
	controllerResp["volts"] = fValue;
	fValue = podPowerController->readAmps(INA260_DC_1);
	controllerResp["amps"] = fValue;
	fValue = podPowerController->readPower(INA260_DC_1);
	controllerResp["watt"] = fValue;

	serializeJson(controllerResp, sResp);
	DBPrintln("sResp : " + sResp);

	res.set("Content-Type", "application/json");
	res.write((uint8_t*)(sResp.c_str()),sResp.length());
}

void podDC2Power(Request &req, Response &res)
{
	JsonDocument controllerResp;
	String sResp;
	int nPercent;
	float fValue;

	fValue = podPowerController->readVolts(INA260_DC_2);
	controllerResp["volts"] = fValue;
	fValue = podPowerController->readAmps(INA260_DC_2);
	controllerResp["amps"] = fValue;
	fValue = podPowerController->readPower(INA260_DC_2);
	controllerResp["watt"] = fValue;

	serializeJson(controllerResp, sResp);
	DBPrintln("sResp : " + sResp);

	res.set("Content-Type", "application/json");
	res.write((uint8_t*)(sResp.c_str()),sResp.length());
}

void podPWM1Power(Request &req, Response &res)
{
	JsonDocument controllerResp;
	String sResp;
	int nPercent;
	float fValue;

	fValue = podPowerController->readVolts(INA260_PWM1);
	controllerResp["volts"] = fValue;
	fValue = podPowerController->readAmps(INA260_PWM1);
	controllerResp["amps"] = fValue;
	fValue = podPowerController->readPower(INA260_PWM1);
	controllerResp["watt"] = fValue;

	serializeJson(controllerResp, sResp);
	DBPrintln("sResp : " + sResp);

	res.set("Content-Type", "application/json");
	res.write((uint8_t*)(sResp.c_str()),sResp.length());
}

void podPWM2Power(Request &req, Response &res)
{
	JsonDocument controllerResp;
	String sResp;
	int nPercent;
	float fValue;

	fValue = podPowerController->readVolts(INA260_PWM2);
	controllerResp["volts"] = fValue;
	fValue = podPowerController->readAmps(INA260_PWM2);
	controllerResp["amps"] = fValue;
	fValue = podPowerController->readPower(INA260_PWM2);
	controllerResp["watt"] = fValue;

	serializeJson(controllerResp, sResp);
	DBPrintln("sResp : " + sResp);

	res.set("Content-Type", "application/json");
	res.write((uint8_t*)(sResp.c_str()),sResp.length());
}

void podBatPower(Request &req, Response &res)
{
	JsonDocument controllerResp;
	String sResp;
	int nPercent;
	float fValue;

	fValue = podPowerController->readVolts(INA260_BAT);
	controllerResp["volts"] = fValue;
	fValue = podPowerController->readAmps(INA260_BAT);
	controllerResp["amps"] = fValue;
	fValue = podPowerController->readPower(INA260_BAT);
	controllerResp["watt"] = fValue;

	serializeJson(controllerResp, sResp);
	DBPrintln("sResp : " + sResp);

	res.set("Content-Type", "application/json");
	res.write((uint8_t*)(sResp.c_str()),sResp.length());
}

void podUsbCPower(Request &req, Response &res)
{
	JsonDocument controllerResp;
	String sResp;
	int nPercent;
	float fValue;

	fValue = podPowerController->readVolts(INA260_USB_C);
	controllerResp["volts"] = fValue;
	fValue = podPowerController->readAmps(INA260_USB_C);
	controllerResp["amps"] = fValue;
	fValue = podPowerController->readPower(INA260_USB_C);
	controllerResp["watt"] = fValue;

	serializeJson(controllerResp, sResp);
	DBPrintln("sResp : " + sResp);

	res.set("Content-Type", "application/json");
	res.write((uint8_t*)(sResp.c_str()),sResp.length());
}

void podMot1Power(Request &req, Response &res)
{
	JsonDocument controllerResp;
	String sResp;
	int nPercent;
	float fValue;

	fValue = podPowerController->readVolts(INA260_VMOT);
	controllerResp["volts"] = fValue;
	fValue = podPowerController->readAmps(INA260_VMOT);
	controllerResp["amps"] = fValue;
	fValue = podPowerController->readPower(INA260_VMOT);
	controllerResp["watt"] = fValue;

	serializeJson(controllerResp, sResp);
	DBPrintln("sResp : " + sResp);

	res.set("Content-Type", "application/json");
	res.write((uint8_t*)(sResp.c_str()),sResp.length());
}

void podMot2Power(Request &req, Response &res)
{
	JsonDocument controllerResp;
	String sResp;
	int nPercent;
	float fValue;

	fValue = podPowerController->readVolts(INA260_VMOT2);
	controllerResp["volts"] = fValue;
	fValue = podPowerController->readAmps(INA260_VMOT2);
	controllerResp["amps"] = fValue;
	fValue = podPowerController->readPower(INA260_VMOT2);
	controllerResp["watt"] = fValue;

	serializeJson(controllerResp, sResp);
	DBPrintln("sResp : " + sResp);

	res.set("Content-Type", "application/json");
	res.write((uint8_t*)(sResp.c_str()),sResp.length());
}

void podType(Request &req, Response &res)
{
	JsonDocument controllerResp;
	String sResp;
	int nPodType;
	Configuration podConfig;

	globalPodConfig->LoadPodConfig(podConfig);

	if(req.method() == Request::PUT) {
		JsonDocument FormData;
		formDataToJson(req, FormData);
		if(FormData.size()==0){
			RestError_x401(controllerResp, res);
			return;
		}
		else {
			if(FormData["value"].is<int>()) {
				nPodType = FormData["value"];
				if(globalPodConfig)
					podConfig.podType = nPodType;
					globalPodConfig->savePodConfig(podConfig);
			}
		}
	}

	controllerResp["value"] = podConfig.podType;
	serializeJson(controllerResp, sResp);
	DBPrintln("sResp : " + sResp);

	res.set("Content-Type", "application/json");
	res.write((uint8_t*)(sResp.c_str()),sResp.length());
}

void RebootTask(void *)
{
	const TickType_t xDelay = 100 / portTICK_PERIOD_MS;

	esp_wifi_stop();
	vTaskDelay(xDelay/2);

	esp_wifi_deinit();
	vTaskDelay(xDelay/2);

	ESP.restart();
}

void podReboot(Request &req, Response &res)
{
	JsonDocument controllerResp;
	String sResp;

	controllerResp["value"] = "Rebooting";
	serializeJson(controllerResp, sResp);
	DBPrintln("sResp : " + sResp);

	res.set("Content-Type", "application/json");
	res.write((uint8_t*)(sResp.c_str()),sResp.length());
	xTaskCreatePinnedToCore(RebootTask, "Reboot", 10000, NULL, 16, NULL,  1);
}

void FactoryRebootTask(void *)
{
	const TickType_t xDelay = 100 / portTICK_PERIOD_MS;
	vTaskDelay(xDelay);
	globalPodConfig->resetAllSettings(); // this reboot the ESP
}
void podFactoryReset(Request &req, Response &res)
{
	JsonDocument controllerResp;
	String sResp;

	controllerResp["value"] = "Resseting to factory default";
	serializeJson(controllerResp, sResp);
	DBPrintln("sResp : " + sResp);

	res.set("Content-Type", "application/json");
	res.write((uint8_t*)(sResp.c_str()),sResp.length());
	xTaskCreatePinnedToCore(FactoryRebootTask, "FactoryRebootTask", 10000, NULL, 16, NULL,  1);
}

void PodAPIServer::startServer()
{
	mRestServer = new NetworkServer(m_nRestPort);
	m_ApiRestServer = new Application();
	DBPrintln("PodAPIServer starting");
	DBPrintln("PodAPIServer Pod UUID : " + String(m_ApiServerUUID.toCharArray()));
	// check if we're connected as client, if not, use AP IP
	mRestServer->begin();
	DBPrintln("m_ApiRestServer mapping endpoints");
	m_ApiRestServer->use("/", &doSetup);
	m_ApiRestServer->use("/setup", &doSetup);

	//
	// adding our own endpoints for the settings and controls
	//

	m_ApiRestServer->use("/setup/useDHCP", &useDHCPState);
	m_ApiRestServer->use("/setup/ipAddress", &ipAddressValue);
	m_ApiRestServer->use("/setup/subnetMask", &subnetMaskValue);
	m_ApiRestServer->use("/setup/ipGateway", &ipGatewayValue);

	// m_ApiRestServer->use("/setup/wifiApSSID", &podHotSpotSSID);
	// m_ApiRestServer->use("/setup/wifiApPassword", &podHotSpotPAssword);
	// m_ApiRestServer->use("/setup/wifiApChannel", &podHotSpotChannel);

	m_ApiRestServer->put("/setup/podCalibrate", &podCalibrate);
	m_ApiRestServer->get("/setup/environmentData", &environmentData);

	// shutter control
	m_ApiRestServer->put("/setup/podOpen", &podOpen);
	m_ApiRestServer->put("/setup/podClose", &podClose);
	m_ApiRestServer->get("/setup/podState", &podState);

	// Power ports control
	m_ApiRestServer->use("/setup/podDC1", &podDC1);
	m_ApiRestServer->use("/setup/podDC2", &podDC2);
	m_ApiRestServer->use("/setup/podPWM1", &podPWM1);
	m_ApiRestServer->use("/setup/podPWM2", &podPWM2);
	m_ApiRestServer->use("/setup/podUsbC", &podUsbC);
	// Power usage
	m_ApiRestServer->get("/setup/podMainPower", &podMainPower);
	m_ApiRestServer->get("/setup/podDC1Power", &podDC1Power);
	m_ApiRestServer->get("/setup/podDC2Power", &podDC2Power);
	m_ApiRestServer->get("/setup/podPWM1Power", &podPWM1Power);
	m_ApiRestServer->get("/setup/podPWM2Power", &podPWM2Power);

	m_ApiRestServer->get("/setup/podBatPower", &podBatPower);
	m_ApiRestServer->get("/setup/podUsbCPower", &podUsbCPower);
	m_ApiRestServer->get("/setup/podMot1Power", &podMot1Power);
	m_ApiRestServer->get("/setup/podMot2Power", &podMot2Power);

	m_ApiRestServer->use("/setup/podType", &podType);

	m_ApiRestServer->get("/setup/serialNumber", &getSerialNumber);

	m_ApiRestServer->put("/setup/reboot", &podReboot);
	m_ApiRestServer->put("/setup/factoryReset", &podFactoryReset);

	DBPrintln("m_ApiRestServer started");
}


void PodAPIServer::checkForRequest()
{
	// process incoming connections one at a time
	NetworkClient client = mRestServer->accept();
	if (client.connected()) {
		m_ApiRestServer->process(&client);
		client.stop();
	}
}

#endif