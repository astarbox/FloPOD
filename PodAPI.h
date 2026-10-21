// ImagingPod API functions
//
//  Created by Rodolphe Pineau on 2026/10/19
//

#pragma message "Alpaca server enabled"
#include <vector>
#include <functional>
#include <ArduinoJson.h>
// Alpaca REST server
#include <UUID.h>
#include <aWOT.h>

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
		nTransactionID++;
	}
}
