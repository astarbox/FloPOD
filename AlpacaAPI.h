// Alpaca API functions
//
//  Created by Rodolphe Pineau on 2024/04/16
//  Copyright © 2024 Rodolphe Pineau. All rights reserved.
//

#pragma message "Alpaca server enabled"
#include <vector>
#include <functional>
#include <ArduinoJson.h>
// Alpaca REST server
#include <UUID.h>
#include <aWOT.h>

#include "podController.h"

#define ALPACA_DISCOVERY_PORT 32227
#define ALPACA_SERVER_PORT 80
#define ALPACA_VAR_BUF_LEN 256
#define ALPACA_OK 0
#define DISCOVERY_ERROR -1
#define DOME_INTERFACE_VERSION 3

#define UDP_PACKET_MAX_SIZE 16


enum AlpacaShutterStates { A_OPEN=0, A_CLOSED, A_OPENING, A_CLOSING,  A_ERROR};
uint32_t nTransactionID;
UUID PodUuid, PodPowerUuid;
String sAlpacaDiscovery = "alpacadiscovery1";
volatile bool bAlpacaConnected = false;

class AlpacaDiscoveryServer
{
public:
	AlpacaDiscoveryServer(int port=ALPACA_DISCOVERY_PORT);
	// AlpacaDiscoveryServer(IPAddress ipAddress, int port=ALPACA_DISCOVERY_PORT);
	void startServer();
	int checkForRequest();
private:
	NetworkUDP *discoveryServer;
	int m_UDPPort;
	IPAddress m_ipAddress;
};

class AlpacaServer
{
public :
	// AlpacaServer(IPAddress ipAddress, int port=ALPACA_SERVER_PORT);
	AlpacaServer(int port=ALPACA_SERVER_PORT);
	void startServer();
	void checkForRequest();
	// void setPodCtrlPtr(RoofClass *pRoof);
private :
	NetworkServer *mRestServer;
	Application  *m_AlpacaRestServer;
	int m_nRestPort;
	// IPAddress m_ipAddress;
};

AlpacaDiscoveryServer *pod_AlpacaDiscoveryServer;
AlpacaServer *pod_AlpacaServer;
// AlpacaDiscoveryServer *podAp_AlpacaDiscoveryServer;
// AlpacaServer *podAp_AlpacaServer;


// ALPACA discovery server

AlpacaDiscoveryServer::AlpacaDiscoveryServer( int port)
{
	m_UDPPort = port;
	discoveryServer = nullptr;
	//	m_ipAddress = ipAddress;
}

void AlpacaDiscoveryServer::startServer()
{
	discoveryServer = new NetworkUDP();
	if(!discoveryServer) {
		discoveryServer = nullptr;
		return;
	}
	discoveryServer->begin(m_ipAddress, m_UDPPort);
	DBPrintln("Alpaca discovery server started on " + IpAddress2String(m_ipAddress) + "port " + String(m_UDPPort));
}

int AlpacaDiscoveryServer::checkForRequest()
{
	if(!discoveryServer)
		return -1;
	String sDiscoveryResponse = "{\"AlpacaPort\":"+String(ALPACA_SERVER_PORT)+"}";
	String sDiscoveryRequest;
	char packetBuffer[UDP_PACKET_MAX_SIZE];
	int packetSize = discoveryServer->parsePacket();
	if (packetSize) {
		DBPrintln("Alpaca discovery server request");
		memset(packetBuffer,0,sizeof(packetBuffer));
		discoveryServer->read(packetBuffer, UDP_PACKET_MAX_SIZE);
		// do stuff
		sDiscoveryRequest = String(packetBuffer);
		DBPrintln("Alpaca discovery server sDiscoveryRequest : " + sDiscoveryRequest);
		if(sDiscoveryRequest.indexOf(sAlpacaDiscovery)==-1) {
			DBPrintln("Alpaca discovery server request error");
			return DISCOVERY_ERROR; // wrong type of discovery message
		}
		DBPrintln("Alpaca discovery server sending response : " + sDiscoveryResponse);
		// send discovery reponse
		discoveryServer->beginPacket(discoveryServer->remoteIP(), discoveryServer->remotePort());
		discoveryServer->write((uint8_t*)sDiscoveryResponse.c_str(), sDiscoveryResponse.length());
		discoveryServer->endPacket();
	}
	return ALPACA_OK;
}

int getAlpacaShutterState()
{
	int nAlpacaShutterState = A_ERROR;
	int nShutterState = IDLE;
	String sTmpString;

	nShutterState = podController->getShutterState();

	switch (nShutterState) {
		case OPEN:
			nAlpacaShutterState = A_OPEN;
			break;
		case CLOSED:
			nAlpacaShutterState = A_CLOSED;
			break;
		case POD_ERROR:
			nAlpacaShutterState = A_ERROR;
			break;
		case OPENING:
		case FINISHING_OPENING:
			nAlpacaShutterState = A_OPENING;
			break;
		case CLOSING:
		case FINISHING_CLOSING:
			nAlpacaShutterState = A_CLOSING;
			break;
		default:
			nAlpacaShutterState = A_ERROR;
			break;

	}
	return nAlpacaShutterState;

}


void formDataToJson(Request &req, JsonDocument &FormData)
{
	char name[ALPACA_VAR_BUF_LEN];
	char value[ALPACA_VAR_BUF_LEN];
	String sName;
	String sValue;

	memset(name,0,ALPACA_VAR_BUF_LEN);
	memset(value,0,ALPACA_VAR_BUF_LEN);
	while(req.form(name, ALPACA_VAR_BUF_LEN-1, value, ALPACA_VAR_BUF_LEN-1)){
		sName  = String(name);
		sName.toLowerCase();
		sValue = String(value);
		sValue.toLowerCase();
		DBPrintln("name : " + sName);
		DBPrintln("value : " + sValue);
		if(isDigit(value[0]) ) {
			if(sValue.indexOf('.') == -1) {
				// int
				FormData[sName]=sValue.toInt();
			} else {
				// double
				FormData[sName]=sValue.toDouble();
			}
		}
		else {
			// string
			// check for boolean
			if(sValue == "true") {
				FormData[sName]=true;
			}
			else if(sValue == "false") {
				FormData[sName]=false;
			}
			else {
				FormData[sName]=sValue;
			}
		}
	}
}


void  getQueryGetVariables(String sQueryString, std::vector<std::vector<String>> &svParameters)
{
	int nErr;
	int nIndex = 0;
	int nCurIndex = 0;
	String sEntry;
	std::vector<String> svKV;
	std::vector<String> svFields;

	DBPrintln("getQueryGetVariables");
	// url parameters are separate by '&'
	while(true) {
		nIndex = sQueryString.indexOf('&',nCurIndex);
		if(nIndex == -1) {
			svFields.push_back(sQueryString.substring(nCurIndex));
			break;
		}
		svFields.push_back(sQueryString.substring(nCurIndex,nIndex));
		nCurIndex = nIndex+1;
	}
	if(svFields.size()) {
		// now split each field in key,value pair with '=' as the separator
		for(String &sTmp : svFields) {
			sTmp.toLowerCase();
			nIndex = sTmp.indexOf('=');
			svKV.push_back(sTmp.substring(0,nIndex));
			svKV.push_back(sTmp.substring(nIndex+1));
			svParameters.push_back(svKV);
			svKV.clear();
		}
	}
	return;
}

bool getIDs(Request &req, JsonDocument &AlpacaResp, JsonDocument &FormData)
{
	char ClientID[64];
	char ClientTransactionID[64];
	String sClientId;
	String sClientTransactionId;
	std::vector<std::vector<String>> svParameters;
	bool bParamOk = true;

	DBPrintln("getIDs");
	AlpacaResp["ServerTransactionID"] = nTransactionID;
	if(req.method() == Request::GET) {
		// the req.query being case sensitive will not work here.
		getQueryGetVariables(String(req.query()), svParameters);
		for( std::vector<String> &svParamEntry : svParameters ) {

			if(svParamEntry.at(0).equals("clientid"))
				sClientId = svParamEntry.at(1);
			if(svParamEntry.at(0).equals("clienttransactionid"))
				sClientTransactionId = svParamEntry.at(1);
		}

		if(sClientId.length())
			AlpacaResp["ClientID"] = sClientId.toInt()<0?0:sClientId.toInt();
		if(sClientTransactionId.length())
			AlpacaResp["ClientTransactionID"] = sClientTransactionId.toInt()<0?0:sClientTransactionId.toInt();
	}
	else { // this is a PUT, therefore there should be some form data
		formDataToJson(req, FormData);
		if(FormData.size()==0){
			bParamOk = false;
		}
		else {
			if(FormData["clientid"].is<unsigned long>()) {
				serializeJson(FormData["clientid"], sClientId);
				sClientId.trim();
				AlpacaResp["ClientID"] = sClientId.toInt()<0?0:sClientId.toInt();
			}
			if(FormData["clienttransactionid"].is<unsigned long>()) {
				serializeJson(FormData["clienttransactionid"], sClientTransactionId);
				sClientTransactionId.trim();
				AlpacaResp["ClientTransactionID"] = sClientTransactionId.toInt()<0?0:sClientTransactionId.toInt();
			}
		}
#ifdef DEBUG
		String sTmp;
		serializeJson(FormData, sTmp);
		DBPrintln("FormData : " + sTmp);
		DBPrintln("FormData.size() : " + String(FormData.size()));
#endif
	}

	DBPrintln("bParamOk : " + String(bParamOk?"Ok":"Error"));
	DBPrintln("sClientId : " + sClientId);
	DBPrintln("sClientTransactionId : " + sClientTransactionId);
	return bParamOk;
}


void getApiVersion(Request &req, Response &res)
{
	JsonDocument AlpacaResp;
	JsonDocument FormData;
	bool bParamsOk = false;
	String sResp;

	DBPrintln("[ ********** getApiVersion ********** ]");
	bParamsOk = getIDs(req, AlpacaResp, FormData);
	res.set("Content-Type", "application/json");
	AlpacaResp["Value"][0] = 1;
	serializeJson(AlpacaResp, sResp);
	DBPrintln("sResp : " + sResp);
	res.write((uint8_t*)(sResp.c_str()),sResp.length());
}

void getDescription(Request &req, Response &res)
{
	JsonDocument AlpacaResp;
	JsonDocument FormData;
	bool bParamsOk = false;
	String sResp;

	DBPrintln("[ ********** getDescription ********** ]");
	bParamsOk = getIDs(req, AlpacaResp, FormData);
	res.set("Content-Type", "application/json");
	AlpacaResp["Value"]["ServerName"]= "FloPod Alpaca";
	AlpacaResp["Value"]["Manufacturer"]= "First Light Optics";
	AlpacaResp["Value"]["ManufacturerVersion"]= VERSION;
	AlpacaResp["Value"]["Location"]= "Earth";
	serializeJson(AlpacaResp, sResp);
	DBPrintln("sResp : " + sResp);
	res.write((uint8_t*)(sResp.c_str()),sResp.length());
}

void getConfiguredDevice(Request &req, Response &res)
{
	JsonDocument AlpacaResp;
	JsonDocument FormData;
	bool bParamsOk = false;
	String sResp;

	DBPrintln("[ ********** getConfiguredDevice ********** ]");
	bParamsOk = getIDs(req, AlpacaResp, FormData);
	res.set("Content-Type", "application/json");
	AlpacaResp["Value"][0] ["DeviceName"]= "Pulsar-Imaging-Pod";
	AlpacaResp["Value"][0] ["DeviceType"]= "dome";
	AlpacaResp["Value"][0] ["DeviceNumber"]= 0;
	AlpacaResp["Value"][0] ["UniqueID"]= PodUuid;

	AlpacaResp["Value"][1] ["DeviceName"]= "Pulsar-Imaging-Pod-Power";
	AlpacaResp["Value"][1] ["DeviceType"]= "switch";
	AlpacaResp["Value"][1] ["DeviceNumber"]= 1;
	AlpacaResp["Value"][1] ["UniqueID"]= PodPowerUuid;

	serializeJson(AlpacaResp, sResp);
	DBPrintln("sResp : " + sResp);
	res.write((uint8_t*)(sResp.c_str()),sResp.length());
}

void doAction(Request &req, Response &res)
{
	JsonDocument AlpacaResp;
	JsonDocument FormData;
	bool bParamsOk = false;
	String sResp;
	String sAction;
	String sParameters;

	DBPrintln("[ ********** doAction ********** ]");
	bParamsOk = getIDs(req, AlpacaResp, FormData);
	res.set("Content-Type", "application/json");
	if(!bParamsOk){
		AlpacaResp["ErrorNumber"] = 0x401;
		AlpacaResp["ErrorMessage"] = "Invalid parameters";
		serializeJson(AlpacaResp, sResp);
		res.write((uint8_t*)(sResp.c_str()),sResp.length());

		return;
	}

	serializeJson(FormData["action"], sAction);
	serializeJson(FormData["parameters"], sParameters);
#ifdef DEBUG
	DBPrintln("sAction : " + sAction);
	DBPrintln("sParameters : " + sParameters);
#endif

	AlpacaResp["ErrorNumber"] = 0;
	AlpacaResp["ErrorMessage"] = "";
	AlpacaResp["Value"] = "Ok";
	serializeJson(AlpacaResp, sResp);
	DBPrintln("sResp : " + sResp);
	res.write((uint8_t*)(sResp.c_str()),sResp.length());
}

void doCommandBlind(Request &req, Response &res)
{
	JsonDocument AlpacaResp;
	JsonDocument FormData;
	bool bParamsOk = false;
	String sResp;
	String sClientId;
	String sClientTransactionId;

	DBPrintln("[ ********** doCommandBlind ********** ]");
	bParamsOk = getIDs(req, AlpacaResp, FormData);
	res.set("Content-Type", "application/json");
	if(!bParamsOk){
		AlpacaResp["ErrorNumber"] = 0x401;
		AlpacaResp["ErrorMessage"] = "Invalid parameters";
		serializeJson(AlpacaResp, sResp);
		res.write((uint8_t*)(sResp.c_str()),sResp.length());

		return;
	}

	AlpacaResp["ErrorNumber"] = 0;
	AlpacaResp["ErrorMessage"] = "";
	serializeJson(AlpacaResp, sResp);
	DBPrintln("sResp : " + sResp);
	res.write((uint8_t*)(sResp.c_str()),sResp.length());
}

void doCommandBool(Request &req, Response &res)
{
	JsonDocument AlpacaResp;
	JsonDocument FormData;
	bool bParamsOk = false;
	String sResp;
	String sClientId;
	String sClientTransactionId;

	DBPrintln("[ ********** doCommandBool ********** ]");
	bParamsOk = getIDs(req, AlpacaResp, FormData);
	res.set("Content-Type", "application/json");
	if(!bParamsOk){
		AlpacaResp["ErrorNumber"] = 0x401;
		AlpacaResp["ErrorMessage"] = "Invalid parameters";
		serializeJson(AlpacaResp, sResp);
		res.write((uint8_t*)(sResp.c_str()),sResp.length());

		return;
	}

	AlpacaResp["ErrorNumber"] = 0;
	AlpacaResp["ErrorMessage"] = "";
	AlpacaResp["Value"] = true;
	serializeJson(AlpacaResp, sResp);
	DBPrintln("sResp : " + sResp);
	res.write((uint8_t*)(sResp.c_str()),sResp.length());
}

void doCommandString(Request &req, Response &res)
{
	JsonDocument AlpacaResp;
	JsonDocument FormData;
	bool bParamsOk = false;
	String sResp;
	String sClientId;
	String sClientTransactionId;

	DBPrintln("[ ********** doCommandString ********** ]");
	bParamsOk = getIDs(req, AlpacaResp, FormData);
	res.set("Content-Type", "application/json");
	if(!bParamsOk){
		AlpacaResp["ErrorNumber"] = 0x401;
		AlpacaResp["ErrorMessage"] = "Invalid parameters";
		serializeJson(AlpacaResp, sResp);
		res.write((uint8_t*)(sResp.c_str()),sResp.length());

		return;
	}

	AlpacaResp["ErrorNumber"] = 0;
	AlpacaResp["ErrorMessage"] = "";
	AlpacaResp["Value"] = "Ok";
	serializeJson(AlpacaResp, sResp);
	DBPrintln("sResp : " + sResp);
	res.write((uint8_t*)(sResp.c_str()),sResp.length());
}

void getConnected(Request &req, Response &res)
{
	JsonDocument AlpacaResp;
	JsonDocument FormData;
	bool bParamsOk = false;
	String sResp;

	DBPrintln("[ ********** getConected ********** ]");
	bParamsOk = getIDs(req, AlpacaResp, FormData);
	res.set("Content-Type", "application/json");
	AlpacaResp["ErrorNumber"] = 0;
	AlpacaResp["ErrorMessage"] = "";
	AlpacaResp["Value"] = bAlpacaConnected;
	serializeJson(AlpacaResp, sResp);
	DBPrintln("sResp : " + sResp);
	res.write((uint8_t*)(sResp.c_str()),sResp.length());
}

void setConnected(Request &req, Response &res)
{
	JsonDocument AlpacaResp;
	JsonDocument FormData;
	bool bParamsOk = false;
	String sResp;
	String sClientId;
	String sClientTransactionId;
	String sParameter;
	String sTmp;

	DBPrintln("[ ********** setConected ********** ]");
	bParamsOk = getIDs(req, AlpacaResp, FormData);
	res.set("Content-Type", "application/json");
	if(!bParamsOk){
		AlpacaResp["ErrorNumber"] = 0x401;
		AlpacaResp["ErrorMessage"] = "Invalid parameters";
		serializeJson(AlpacaResp, sResp);
		res.write((uint8_t*)(sResp.c_str()),sResp.length());

		return;
	}

	if(!FormData["connected"].is<bool>()) {
		AlpacaResp["ErrorNumber"] = 0x401;
		AlpacaResp["ErrorMessage"] = "Invalid parameters, missing 'Connected'";
		serializeJson(AlpacaResp, sResp);
		res.write((uint8_t*)(sResp.c_str()),sResp.length());

		return;
	}

	bAlpacaConnected = FormData["connected"];
	DBPrintln("bAlpacaConnected : " + (bAlpacaConnected?String("true"):String("false")));
	AlpacaResp["ErrorNumber"] = 0;
	AlpacaResp["ErrorMessage"] = "";
	serializeJson(AlpacaResp, sResp);
	DBPrintln("sResp : " + sResp);
	res.write((uint8_t*)(sResp.c_str()),sResp.length());
}

void domeConnect(Request &req, Response &res)
{
	JsonDocument AlpacaResp;
	JsonDocument FormData;
	bool bParamsOk = false;
	String sResp;
	String sClientId;
	String sClientTransactionId;
	String sParameter;
	String sTmp;

	DBPrintln("[ ********** setConected ********** ]");
	bParamsOk = getIDs(req, AlpacaResp, FormData);
	res.set("Content-Type", "application/json");

	if(!bParamsOk){
		AlpacaResp["ErrorNumber"] = 0x401;
		AlpacaResp["ErrorMessage"] = "Invalid parameters";
		serializeJson(AlpacaResp, sResp);
		res.write((uint8_t*)(sResp.c_str()),sResp.length());
			return;
	}

	bAlpacaConnected = true;
	DBPrintln("bAlpacaConnected : " + (bAlpacaConnected?String("true"):String("false")));

	AlpacaResp["ErrorNumber"] = 0;
	AlpacaResp["ErrorMessage"] = "";
	serializeJson(AlpacaResp, sResp);
	DBPrintln("sResp : " + sResp);

	res.write((uint8_t*)(sResp.c_str()),sResp.length());
}

void domeConnecting(Request &req, Response &res)
{
	JsonDocument AlpacaResp;
	JsonDocument FormData;
	bool bParamsOk = false;
	String sResp;
	String sClientId;
	String sClientTransactionId;
	String sParameter;
	String sTmp;

	DBPrintln("[ ********** setConected ********** ]");
	bParamsOk = getIDs(req, AlpacaResp, FormData);
	res.set("Content-Type", "application/json");

	if(!bParamsOk){
		AlpacaResp["ErrorNumber"] = 0x401;
		AlpacaResp["ErrorMessage"] = "Invalid parameters";
		serializeJson(AlpacaResp, sResp);
		res.write((uint8_t*)(sResp.c_str()),sResp.length());
			return;
	}

	AlpacaResp["ErrorNumber"] = 0;
	AlpacaResp["ErrorMessage"] = "";
	AlpacaResp["Value"] = false; // it's already connected
	serializeJson(AlpacaResp, sResp);
	DBPrintln("sResp : " + sResp);

	res.write((uint8_t*)(sResp.c_str()),sResp.length());
}


void getDomeState(Request &req, Response &res)
{
	JsonDocument AlpacaResp;
	JsonDocument jsTmp;
	JsonDocument FormData;
	bool bParamsOk = false;
	String sResp;
	float Alt, Az;
	float dParkPos, dCurrentAz;
	bool bParked = false;
	int nState;

	DBPrintln("[ ********** getDomeState ********** ]");
	bParamsOk = getIDs(req, AlpacaResp, FormData);

	res.set("Content-Type", "application/json");
	AlpacaResp["ErrorNumber"] = 0;
	AlpacaResp["ErrorMessage"] = "";

	// add states to response

	nState = podController->getShutterState();
	if(nState == OPEN)
		Alt = 90.0f;
	else if (nState == CLOSED)
		Alt = 0.0f;
	else {
		Alt = podController->getAltitude();
	}
	jsTmp["Name"] = "Altitude";
	jsTmp["Value"] = Alt;
	AlpacaResp["Value"].add(jsTmp);
	jsTmp.clear();

	jsTmp["Name"] = "Azimuth";
	jsTmp["Value"] = podController->GetAzimuth();
	AlpacaResp["Value"].add(jsTmp);
	jsTmp.clear();

	jsTmp["Name"] = "ShutterStatus";
	jsTmp["Value"] = getAlpacaShutterState();
	AlpacaResp["Value"].add(jsTmp);
	jsTmp.clear();


	jsTmp["Name"] = "Slewing";
	if(podController->getShutterState() != IDLE) {
		jsTmp["Value"] = true;
	}
	else {
		jsTmp["Value"] = false;
	}
	AlpacaResp["Value"].add(jsTmp);

	serializeJson(AlpacaResp, sResp);
	DBPrintln("sResp : " + sResp);

	res.write((uint8_t*)(sResp.c_str()),sResp.length());
}

void domeDisconnect(Request &req, Response &res)
{
	JsonDocument AlpacaResp;
	JsonDocument FormData;
	bool bParamsOk = false;
	String sResp;
	String sClientId;
	String sClientTransactionId;
	String sParameter;
	String sTmp;

	DBPrintln("[ ********** setConected ********** ]");
	bParamsOk = getIDs(req, AlpacaResp, FormData);
	res.set("Content-Type", "application/json");

	if(!bParamsOk){
		AlpacaResp["ErrorNumber"] = 0x401;
		AlpacaResp["ErrorMessage"] = "Invalid parameters";
		serializeJson(AlpacaResp, sResp);
		res.write((uint8_t*)(sResp.c_str()),sResp.length());
			return;
	}

	bAlpacaConnected = false;
	DBPrintln("bAlpacaConnected : " + (bAlpacaConnected?String("true"):String("false")));

	AlpacaResp["ErrorNumber"] = 0;
	AlpacaResp["ErrorMessage"] = "";
	serializeJson(AlpacaResp, sResp);
	DBPrintln("sResp : " + sResp);

	res.write((uint8_t*)(sResp.c_str()),sResp.length());
}

void getDeviceDescription(Request &req, Response &res)
{
	JsonDocument AlpacaResp;
	JsonDocument FormData;
	bool bParamsOk = false;
	String sResp;
	DBPrintln("[ ********** getDeviceDescription ********** ]");
	bParamsOk = getIDs(req, AlpacaResp, FormData);
	res.set("Content-Type", "application/json");
	AlpacaResp["ErrorNumber"] = 0;
	AlpacaResp["ErrorMessage"] = "";
	AlpacaResp["Value"]= "RTI-Zone dome controller";
	serializeJson(AlpacaResp, sResp);
	DBPrintln("sResp : " + sResp);
	res.write((uint8_t*)(sResp.c_str()),sResp.length());
}

void getDriverInfo(Request &req, Response &res)
{
	JsonDocument AlpacaResp;
	JsonDocument FormData;
	bool bParamsOk = false;
	String sResp;
	DBPrintln("[ ********** getDriverInfo ********** ]");
	bParamsOk = getIDs(req, AlpacaResp, FormData);
	res.set("Content-Type", "application/json");
	AlpacaResp["ErrorNumber"] = 0;
	AlpacaResp["ErrorMessage"] = "";
	AlpacaResp["Value"]= "RTI-Zone Dome controller";
	serializeJson(AlpacaResp, sResp);
	DBPrintln("sResp : " + sResp);
	res.write((uint8_t*)(sResp.c_str()),sResp.length());
}

void getDriverVersion(Request &req, Response &res)
{
	JsonDocument AlpacaResp;
	JsonDocument FormData;
	bool bParamsOk = false;
	String sResp;
	DBPrintln("[ ********** getDriverVersion ********** ]");
	bParamsOk = getIDs(req, AlpacaResp, FormData);
	res.set("Content-Type", "application/json");
	AlpacaResp["ErrorNumber"] = 0;
	AlpacaResp["ErrorMessage"] = "";
	AlpacaResp["Value"]= String(VERSION);
	serializeJson(AlpacaResp, sResp);
	DBPrintln("sResp : " + sResp);
	res.write((uint8_t*)(sResp.c_str()),sResp.length());
}

void getInterfaceVersion(Request &req, Response &res)
{
	JsonDocument AlpacaResp;
	JsonDocument FormData;
	bool bParamsOk = false;
	String sResp;
	DBPrintln("[ ********** getInterfaceVersion ********** ]");
	bParamsOk = getIDs(req, AlpacaResp, FormData);
	res.set("Content-Type", "application/json");
	AlpacaResp["ErrorNumber"] = 0;
	AlpacaResp["ErrorMessage"] = "";
	AlpacaResp["Value"]= DOME_INTERFACE_VERSION;
	serializeJson(AlpacaResp, sResp);
	DBPrintln("sResp : " + sResp);
	res.write((uint8_t*)(sResp.c_str()),sResp.length());
}

void getName(Request &req, Response &res)
{
	JsonDocument AlpacaResp;
	JsonDocument FormData;
	bool bParamsOk = false;
	String sResp;
	DBPrintln("[ ********** getName ********** ]");
	bParamsOk = getIDs(req, AlpacaResp, FormData);
	res.set("Content-Type", "application/json");
	AlpacaResp["ErrorNumber"] = 0;
	AlpacaResp["ErrorMessage"] = "";
	AlpacaResp["Value"]= "RTI-Zone Dome controller";
	serializeJson(AlpacaResp, sResp);
	DBPrintln("sResp : " + sResp);
	res.write((uint8_t*)(sResp.c_str()),sResp.length());
}

void getSupportedActions(Request &req, Response &res)
{
	JsonDocument AlpacaResp;
	JsonDocument FormData;
	bool bParamsOk = false;
	String sResp;

	DBPrintln("[ ********** getSupportedActions ********** ]");
	bParamsOk = getIDs(req, AlpacaResp, FormData);

	res.set("Content-Type", "application/json");

	AlpacaResp["Value"] = "[]";
	AlpacaResp["ErrorNumber"] = 0;
	AlpacaResp["ErrorMessage"] = "";
	serializeJson(AlpacaResp, sResp);
	DBPrintln("sResp : " + sResp);

	res.write((uint8_t*)(sResp.c_str()),sResp.length());
}

void getAltitude(Request &req, Response &res)
{
	JsonDocument AlpacaResp;
	JsonDocument FormData;
	bool bParamsOk = false;
	podStates nState;
	String sResp;
	DBPrintln("[ ********** getAltitude ********** ]");
	bParamsOk = getIDs(req, AlpacaResp, FormData);
	res.set("Content-Type", "application/json");
	AlpacaResp["ErrorNumber"] = 0;
	AlpacaResp["ErrorMessage"] = "";
	nState = podController->getShutterState();
	if(nState == OPEN)
		AlpacaResp["Value"] = 90.0f;
	else if (nState == CLOSED)
		AlpacaResp["Value"] = 90.0f;
	else {
		AlpacaResp["Value"] = podController->getAltitude();
	}
	serializeJson(AlpacaResp, sResp);
	DBPrintln("sResp : " + sResp);
	res.write((uint8_t*)(sResp.c_str()),sResp.length());
}

void geAtHome(Request &req, Response &res)
{
	JsonDocument AlpacaResp;
	JsonDocument FormData;
	bool bParamsOk = false;
	String sResp;
	DBPrintln("[ ********** geAtHome ********** ]");
	bParamsOk = getIDs(req, AlpacaResp, FormData);
	res.set("Content-Type", "application/json");
	AlpacaResp["ErrorNumber"] = 0;
	AlpacaResp["ErrorMessage"] = "";
	/*if(String(podController->GetHomeStatus() == CLOSED)) {
		AlpacaResp["Value"] = true;
	}
	else {
		AlpacaResp["Value"] = false;
	}
	*/
	serializeJson(AlpacaResp, sResp);
	DBPrintln("sResp : " + sResp);
	res.write((uint8_t*)(sResp.c_str()),sResp.length());
}

void geAtPark(Request &req, Response &res)
{
	JsonDocument AlpacaResp;
	JsonDocument FormData;
	bool bParamsOk = false;
	String sResp;
	DBPrintln("[ ********** geAtPark ********** ]");
	bParamsOk = getIDs(req, AlpacaResp, FormData);
	res.set("Content-Type", "application/json");
	if(bParked) {
		AlpacaResp["Value"] = true;
	}
	else {
		AlpacaResp["Value"] = false;
	}
	serializeJson(AlpacaResp, sResp);
	DBPrintln("sResp : " + sResp);
	res.write((uint8_t*)(sResp.c_str()),sResp.length());
}

void getAzimuth(Request &req, Response &res)
{
	JsonDocument AlpacaResp;
	JsonDocument FormData;
	bool bParamsOk = false;
	String sResp;
	DBPrintln("[ ********** getAzimuth ********** ]");
	bParamsOk = getIDs(req, AlpacaResp, FormData);
	res.set("Content-Type", "application/json");
	AlpacaResp["ErrorNumber"] = 0;
	AlpacaResp["ErrorMessage"] = "";
	AlpacaResp["Value"] = podController->GetAzimuth();
	serializeJson(AlpacaResp, sResp);
	DBPrintln("sResp : " + sResp);
	res.write((uint8_t*)(sResp.c_str()),sResp.length());
}

void canfindhome(Request &req, Response &res)
{
	JsonDocument AlpacaResp;
	JsonDocument FormData;
	bool bParamsOk = false;
	String sResp;
	DBPrintln("[ ********** canfindhome ********** ]");
	bParamsOk = getIDs(req, AlpacaResp, FormData);
	res.set("Content-Type", "application/json");
	AlpacaResp["ErrorNumber"] = 0;
	AlpacaResp["ErrorMessage"] = "";
	AlpacaResp["Value"] = true;
	serializeJson(AlpacaResp, sResp);
	DBPrintln("sResp : " + sResp);
	res.write((uint8_t*)(sResp.c_str()),sResp.length());
}

void canPark(Request &req, Response &res)
{
	JsonDocument AlpacaResp;
	JsonDocument FormData;
	bool bParamsOk = false;
	String sResp;
	DBPrintln("[ ********** canPark ********** ]");
	bParamsOk = getIDs(req, AlpacaResp, FormData);
	res.set("Content-Type", "application/json");
	AlpacaResp["ErrorNumber"] = 0;
	AlpacaResp["ErrorMessage"] = "";
	AlpacaResp["Value"] = false;
	serializeJson(AlpacaResp, sResp);
	DBPrintln("sResp : " + sResp);
	res.write((uint8_t*)(sResp.c_str()),sResp.length());
}

void canSetAltitude(Request &req, Response &res)
{
	JsonDocument AlpacaResp;
	JsonDocument FormData;
	bool bParamsOk = false;
	String sResp;
	DBPrintln("[ ********** canSetAltitude ********** ]");
	bParamsOk = getIDs(req, AlpacaResp, FormData);
	res.set("Content-Type", "application/json");
	AlpacaResp["ErrorNumber"] = 0;
	AlpacaResp["ErrorMessage"] = "";
	AlpacaResp["Value"] = false;
	serializeJson(AlpacaResp, sResp);
	DBPrintln("sResp : " + sResp);
	res.write((uint8_t*)(sResp.c_str()),sResp.length());
}

void canSetAzimuth(Request &req, Response &res)
{
	JsonDocument AlpacaResp;
	JsonDocument FormData;
	bool bParamsOk = false;
	String sResp;
	DBPrintln("[ ********** canSetAzimuth ********** ]");
	bParamsOk = getIDs(req, AlpacaResp, FormData);
	res.set("Content-Type", "application/json");
	AlpacaResp["ErrorNumber"] = 0;
	AlpacaResp["ErrorMessage"] = "";
	AlpacaResp["Value"] = true;
	serializeJson(AlpacaResp, sResp);
	DBPrintln("sResp : " + sResp);
	res.write((uint8_t*)(sResp.c_str()),sResp.length());
}

void canSetPark(Request &req, Response &res)
{
	JsonDocument AlpacaResp;
	JsonDocument FormData;
	bool bParamsOk = false;
	String sResp;
	DBPrintln("[ ********** canSetPark ********** ]");
	bParamsOk = getIDs(req, AlpacaResp, FormData);
	res.set("Content-Type", "application/json");
	AlpacaResp["ErrorNumber"] = 0;
	AlpacaResp["ErrorMessage"] = "";
	AlpacaResp["Value"] = true;
	serializeJson(AlpacaResp, sResp);
	DBPrintln("sResp : " + sResp);
	res.write((uint8_t*)(sResp.c_str()),sResp.length());
}

void canSetShutter(Request &req, Response &res)
{
	JsonDocument AlpacaResp;
	JsonDocument FormData;
	bool bParamsOk = false;
	String sResp;
	DBPrintln("[ ********** canSetShutter ********** ]");
	bParamsOk = getIDs(req, AlpacaResp, FormData);
	res.set("Content-Type", "application/json");
	AlpacaResp["ErrorNumber"] = 0;
	AlpacaResp["ErrorMessage"] = "";
	AlpacaResp["Value"] = true;
	serializeJson(AlpacaResp, sResp);
	DBPrintln("sResp : " + sResp);
	res.write((uint8_t*)(sResp.c_str()),sResp.length());
}

void canSlave(Request &req, Response &res)
{
	JsonDocument AlpacaResp;
	JsonDocument FormData;
	bool bParamsOk = false;
	String sResp;
	DBPrintln("[ ********** canSlave ********** ]");
	bParamsOk = getIDs(req, AlpacaResp, FormData);
	res.set("Content-Type", "application/json");
	AlpacaResp["ErrorNumber"] = 0;
	AlpacaResp["ErrorMessage"] = "";
	AlpacaResp["Value"] = false;
	serializeJson(AlpacaResp, sResp);
	DBPrintln("sResp : " + sResp);
	res.write((uint8_t*)(sResp.c_str()),sResp.length());
}

void canSyncAzimuth(Request &req, Response &res)
{
	JsonDocument AlpacaResp;
	JsonDocument FormData;
	bool bParamsOk = false;
	String sResp;
	DBPrintln("[ ********** canSyncAzimuth ********** ]");
	bParamsOk = getIDs(req, AlpacaResp, FormData);
	res.set("Content-Type", "application/json");
	AlpacaResp["ErrorNumber"] = 0;
	AlpacaResp["ErrorMessage"] = "";
	AlpacaResp["Value"] = true;
	serializeJson(AlpacaResp, sResp);
	DBPrintln("sResp : " + sResp);
	res.write((uint8_t*)(sResp.c_str()),sResp.length());
}

void getShutterStatus(Request &req, Response &res)
{
	JsonDocument AlpacaResp;
	JsonDocument FormData;
	bool bParamsOk = false;
	String sResp;
	podStates nState;

	DBPrintln("[ ********** getShutterStatus ********** ]");
	bParamsOk = getIDs(req, AlpacaResp, FormData);
	res.set("Content-Type", "application/json");
	AlpacaResp["ErrorNumber"] = 0;
	AlpacaResp["ErrorMessage"] = "";
	nState = podController->getShutterState();
	switch (nState) {
		case OPEN:
			AlpacaResp["Value"] = A_OPEN;
			break;
		case CLOSED:
			AlpacaResp["Value"] = A_CLOSED;
			break;
		case POD_ERROR:
			AlpacaResp["Value"] = A_ERROR;
			break;
		case OPENING:
		case FINISHING_OPENING:
			AlpacaResp["Value"] = A_OPENING;
			break;
		case CLOSING:
		case FINISHING_CLOSING:
			AlpacaResp["Value"] = A_CLOSING;
			break;
		default:
			AlpacaResp["Value"] = A_ERROR;
			break;
	}
	serializeJson(AlpacaResp, sResp);
	DBPrintln("sResp : " + sResp);
	res.write((uint8_t*)(sResp.c_str()),sResp.length());
}
void getSlaved(Request &req, Response &res)
{
	JsonDocument AlpacaResp;
	JsonDocument FormData;
	bool bParamsOk = false;
	String sResp;
	DBPrintln("[ ********** canSlave ********** ]");
	bParamsOk = getIDs(req, AlpacaResp, FormData);
	res.set("Content-Type", "application/json");
	AlpacaResp["ErrorNumber"] = 0;
	AlpacaResp["ErrorMessage"] = "";
	AlpacaResp["Value"] = false;
	serializeJson(AlpacaResp, sResp);
	DBPrintln("sResp : " + sResp);
	res.write((uint8_t*)(sResp.c_str()),sResp.length());
}

void setSlaved(Request &req, Response &res)
{
	JsonDocument AlpacaResp;
	JsonDocument FormData;
	bool bParamsOk = false;
	String sResp;
	DBPrintln("[ ********** Slaved ********** ]");
	bParamsOk = getIDs(req, AlpacaResp, FormData);
	res.set("Content-Type", "application/json");
	AlpacaResp["ErrorNumber"] = 0x400;
	AlpacaResp["ErrorMessage"] = "Invalid parameters, missing 'Connected'";
	AlpacaResp["Value"] = false;
	serializeJson(AlpacaResp, sResp);
	res.write((uint8_t*)(sResp.c_str()),sResp.length());
}

void getSlewing(Request &req, Response &res)
{
	JsonDocument AlpacaResp;
	JsonDocument FormData;
	bool bParamsOk = false;
	String sResp;
	DBPrintln("[ ********** getSlewing ********** ]");
	bParamsOk = getIDs(req, AlpacaResp, FormData);
	res.set("Content-Type", "application/json");
	AlpacaResp["ErrorNumber"] = 0;
	AlpacaResp["ErrorMessage"] = "";
	DBPrintln("RoofState : " + String(podController->getShutterState()));

	if(podController->getShutterState() != IDLE) {
		AlpacaResp["Value"] = true;
	}
	else {
		AlpacaResp["Value"] = false;
	}

	serializeJson(AlpacaResp, sResp);
	DBPrintln("sResp : " + sResp);
	res.write((uint8_t*)(sResp.c_str()),sResp.length());
}

void doAbort(Request &req, Response &res)
{
	JsonDocument AlpacaResp;
	JsonDocument FormData;
	bool bParamsOk = false;
	String sResp;
	DBPrintln("[ ********** doAbort ********** ]");
	bParamsOk = getIDs(req, AlpacaResp, FormData);
	res.set("Content-Type", "application/json");
	if(!bParamsOk){
		AlpacaResp["ErrorNumber"] = 0x401;
		AlpacaResp["ErrorMessage"] = "Invalid parameters";
		serializeJson(AlpacaResp, sResp);
		res.write((uint8_t*)(sResp.c_str()),sResp.length());

		return;
	}

	AlpacaResp["ErrorNumber"] = 0;
	AlpacaResp["ErrorMessage"] = "";
	podController->Abort(); // this is in the RoREth-esp32.ino

	serializeJson(AlpacaResp, sResp);
	DBPrintln("sResp : " + sResp);
	res.write((uint8_t*)(sResp.c_str()),sResp.length());
}

void doCloseShutter(Request &req, Response &res)
{
	JsonDocument AlpacaResp;
	JsonDocument FormData;
	bool bParamsOk = false;
	String sResp;

	DBPrintln("[ ********** doCloseShutter ********** ]");
	bParamsOk = getIDs(req, AlpacaResp, FormData);
	res.set("Content-Type", "application/json");
	if(!bParamsOk){
		AlpacaResp["ErrorNumber"] = 0x401;
		AlpacaResp["ErrorMessage"] = "Invalid parameters";
		serializeJson(AlpacaResp, sResp);
		res.write((uint8_t*)(sResp.c_str()),sResp.length());

		return;
	}

	AlpacaResp["ErrorNumber"] = 0;
	AlpacaResp["ErrorMessage"] = "";
	podController->Close();
	serializeJson(AlpacaResp, sResp);
	DBPrintln("sResp : " + sResp);
	res.write((uint8_t*)(sResp.c_str()),sResp.length());
}

void doFindHome(Request &req, Response &res)
{
	JsonDocument AlpacaResp;
	JsonDocument FormData;
	bool bParamsOk = false;
	String sResp;
	DBPrintln("[ ********** doFindHome ********** ]");
	bParamsOk = getIDs(req, AlpacaResp, FormData);
	res.set("Content-Type", "application/json");
	if(!bParamsOk){
		AlpacaResp["ErrorNumber"] = 0x401;
		AlpacaResp["ErrorMessage"] = "Invalid parameters";
		serializeJson(AlpacaResp, sResp);
		res.write((uint8_t*)(sResp.c_str()),sResp.length());

		return;
	}


	AlpacaResp["ErrorNumber"] = 0;
	AlpacaResp["ErrorMessage"] = "";
	serializeJson(AlpacaResp, sResp);
	DBPrintln("sResp : " + sResp);
	res.write((uint8_t*)(sResp.c_str()),sResp.length());
}

void doOpenShutter(Request &req, Response &res)
{
	JsonDocument AlpacaResp;
	JsonDocument FormData;
	bool bParamsOk = false;
	String sResp;

	DBPrintln("[ ********** doOpenShutter ********** ]");
	bParamsOk = getIDs(req, AlpacaResp, FormData);
	res.set("Content-Type", "application/json");
	if(!bParamsOk){
		AlpacaResp["ErrorNumber"] = 0x401;
		AlpacaResp["ErrorMessage"] = "Invalid parameters";
		serializeJson(AlpacaResp, sResp);
		res.write((uint8_t*)(sResp.c_str()),sResp.length());

		return;
	}

	AlpacaResp["ErrorNumber"] = 0;
	AlpacaResp["ErrorMessage"] = "";
	podController->Open();

	serializeJson(AlpacaResp, sResp);
	DBPrintln("sResp : " + sResp);
	res.write((uint8_t*)(sResp.c_str()),sResp.length());
}

void doPark(Request &req, Response &res)
{
	JsonDocument AlpacaResp;
	JsonDocument FormData;
	bool bParamsOk = false;
	String sResp;
	double fParkPos;
	DBPrintln("[ ********** doPark ********** ]");
	bParamsOk = getIDs(req, AlpacaResp, FormData);
	res.set("Content-Type", "application/json");
	if(!bParamsOk){
		AlpacaResp["ErrorNumber"] = 0x401;
		AlpacaResp["ErrorMessage"] = "Invalid parameters";
		serializeJson(AlpacaResp, sResp);
		res.write((uint8_t*)(sResp.c_str()),sResp.length());

		return;
	}

	bParked = true;
	AlpacaResp["ErrorNumber"] = 0;
	AlpacaResp["ErrorMessage"] = "";
	serializeJson(AlpacaResp, sResp);
	DBPrintln("sResp : " + sResp);
	res.write((uint8_t*)(sResp.c_str()),sResp.length());
}

void setPark(Request &req, Response &res)
{
	JsonDocument AlpacaResp;
	JsonDocument FormData;
	bool bParamsOk = false;
	String sResp;
	double fParkPos;
	DBPrintln("[ ********** setPark ********** ]");
	bParamsOk = getIDs(req, AlpacaResp, FormData);
	res.set("Content-Type", "application/json");
	if(!bParamsOk){
		AlpacaResp["ErrorNumber"] = 0x401;
		AlpacaResp["ErrorMessage"] = "Invalid parameters";
		serializeJson(AlpacaResp, sResp);
		res.write((uint8_t*)(sResp.c_str()),sResp.length());

		return;
	}


	AlpacaResp["ErrorNumber"] = 0;
	AlpacaResp["ErrorMessage"] = "";
	serializeJson(AlpacaResp, sResp);
	DBPrintln("sResp : " + sResp);
	res.write((uint8_t*)(sResp.c_str()),sResp.length());
	fParkPos = podController->GetAzimuth();
	podController->SetParkAzimuth(fParkPos);
}

void doAltitudeSlew(Request &req, Response &res)
{
	JsonDocument AlpacaResp;
	JsonDocument FormData;
	bool bParamsOk = false;
	String sResp;
	DBPrintln("[ ********** doAltitudeSlew ********** ]");
	bParamsOk = getIDs(req, AlpacaResp, FormData);
	res.set("Content-Type", "application/json");
	if(!bParamsOk){
		AlpacaResp["ErrorNumber"] = 0x401;
		AlpacaResp["ErrorMessage"] = "Invalid parameters";
		serializeJson(AlpacaResp, sResp);
		res.write((uint8_t*)(sResp.c_str()),sResp.length());

		return;
	}

	if(!FormData["altitude"].is<double>()) {
		AlpacaResp["ErrorNumber"] = 0x401;
		AlpacaResp["ErrorMessage"] = "Invalid value";
		serializeJson(AlpacaResp, sResp);
		res.write((uint8_t*)(sResp.c_str()),sResp.length());

		return;
	}
	// in case we implement this one day.
	AlpacaResp["ErrorNumber"] = 0x400;
	AlpacaResp["ErrorMessage"] = "Not implemented";
	serializeJson(AlpacaResp, sResp);
	DBPrintln("sResp : " + sResp);
	res.write((uint8_t*)(sResp.c_str()),sResp.length());
}

void doGoTo(Request &req, Response &res)
{
	JsonDocument AlpacaResp;
	JsonDocument FormData;
	bool bParamsOk = false;
	String sResp;
	double dNewPos;
	DBPrintln("[ ********** doGoTo ********** ]");
	bParamsOk = getIDs(req, AlpacaResp, FormData);
	res.set("Content-Type", "application/json");
	if(!bParamsOk){
		AlpacaResp["ErrorNumber"] = 0x401;
		AlpacaResp["ErrorMessage"] = "Invalid parameters";
		serializeJson(AlpacaResp, sResp);
		res.write((uint8_t*)(sResp.c_str()),sResp.length());

		return;
	}


	if(!FormData["azimuth"].is<double>()) {
		AlpacaResp["ErrorNumber"] = 0x401;
		AlpacaResp["ErrorMessage"] = "Invalid parameters";
		serializeJson(AlpacaResp, sResp);
		res.write((uint8_t*)(sResp.c_str()),sResp.length());
		return;
	}

	dNewPos = FormData["azimuth"];
	if(dNewPos < 0 || dNewPos>360) {
		AlpacaResp["ErrorNumber"] = 1025;
		AlpacaResp["ErrorMessage"] = "Invalid azimuth";
		serializeJson(AlpacaResp, sResp);
		res.write((uint8_t*)(sResp.c_str()),sResp.length());

		return;
	}

	podController->GoToAzimuth(dNewPos);
	AlpacaResp["ErrorNumber"] = 0;
	AlpacaResp["ErrorMessage"] = "";
	serializeJson(AlpacaResp, sResp);
	DBPrintln("sResp : " + sResp);
	res.write((uint8_t*)(sResp.c_str()),sResp.length());
}

void doSyncAzimuth(Request &req, Response &res)
{
	JsonDocument AlpacaResp;
	JsonDocument FormData;
	bool bParamsOk = false;
	String sResp;
	double dNewPos;
	DBPrintln("[ ********** doSyncAzimuth ********** ]");
	bParamsOk = getIDs(req, AlpacaResp, FormData);
	res.set("Content-Type", "application/json");
	if(!bParamsOk){
		AlpacaResp["ErrorNumber"] = 0x401;
		AlpacaResp["ErrorMessage"] = "Invalid parameters";
		serializeJson(AlpacaResp, sResp);
		res.write((uint8_t*)(sResp.c_str()),sResp.length());

		return;
	}

	if(!FormData["azimuth"].is<double>()) {
		AlpacaResp["ErrorNumber"] = 1025;
		AlpacaResp["ErrorMessage"] = "Invalid azimuth";
		serializeJson(AlpacaResp, sResp);
		res.write((uint8_t*)(sResp.c_str()),sResp.length());

		return;
	}

	dNewPos = FormData["azimuth"];
	if(dNewPos<0 || dNewPos > 360) {
		AlpacaResp["ErrorNumber"] = 0x401;
		AlpacaResp["ErrorMessage"] = "Invalid Azimuth";
		serializeJson(AlpacaResp, sResp);
		res.write((uint8_t*)(sResp.c_str()),sResp.length());

		return;
	}

	podController->SyncPosition(dNewPos);
	AlpacaResp["ErrorNumber"] = 0;
	AlpacaResp["ErrorMessage"] = "";
	serializeJson(AlpacaResp, sResp);
	DBPrintln("sResp : " + sResp);
	res.write((uint8_t*)(sResp.c_str()),sResp.length());
}


void doSetup(Request &req, Response &res)
{
	JsonDocument AlpacaResp;
	JsonDocument FormData;
	bool bParamsOk = false;
	String sResp;
	String sHTML;
	res.set("Content-Type", "text/html");
	DBPrintln("[ ********** doSetup ********** ]");
	bParamsOk = getIDs(req, AlpacaResp, FormData);
	sHTML = "<!DOCTYPE html>\n<html>\n";
	sHTML += "<head>";
	sHTML += "<title>RTI Dome Setup</title>\n";
	sHTML += "</head>\n";
	sHTML += "<body>\n";
	sHTML += "<H1>RTI Dome Setup</H1>\n";
	// display passed data
	if(FormData.size()!=0){
		sHTML += "<p>data passed : </p>\n";
		sHTML += "<p>"+sResp+"</p>\n";
	}

	sHTML += "</body>\n</html>\n";
	res.print(sHTML);
}

//
// controller settings API
//
/*
void subnetMaskValue(Request &req, Response &res)
{
	JsonDocument controllerResp;
	String sResp;

	if(req.method() == Request::PUT) {
		JsonDocument FormData;
		formDataToJson(req, FormData);
		if(FormData.size()==0){
		}
		else {
			if(FormData["value"].is<String>()) {
				podController->setIPSubnetMask(FormData["value"]);
			}
		}
	}

	controllerResp["value"] = String(RoofClass::IpAddress2String(domeEthernet.subnetMask()));
	serializeJson(controllerResp, sResp);
	DBPrintln("sResp : " + sResp);

	res.set("Content-Type", "application/json");
	res.write((uint8_t*)(sResp.c_str()),sResp.length());
}

void ipGetewayValue(Request &req, Response &res)
{
	JsonDocument controllerResp;
	String sResp;

	if(req.method() == Request::PUT) {
		JsonDocument FormData;
		formDataToJson(req, FormData);
		if(FormData.size()==0){
		}
		else {
			if(FormData["value"].is<String>()) {
				podController->setIPGateway(FormData["value"]);
			}
		}
	}

	controllerResp["value"] = String(RoofClass::IpAddress2String(domeEthernet.gatewayIP()));
	serializeJson(controllerResp, sResp);
	DBPrintln("sResp : " + sResp);

	res.set("Content-Type", "application/json");
	res.write((uint8_t*)(sResp.c_str()),sResp.length());
}

void roofCalibrateAction(Request &req, Response &res)
{
	JsonDocument controllerResp;
	String sResp;

	if(req.method() == Request::PUT) {
		JsonDocument FormData;
		formDataToJson(req, FormData);
		if(FormData.size()==0){
		}
		else {
			if(FormData["value"].is<String>()) {
				if(FormData["value"] == "start") {
					podController->StartCalibrating();
				}
				if(FormData["value"] == "abort") {
					podController->motorStop();
				}
			}
		}
	}

	controllerResp["value"] = String(RoofClass::IpAddress2String(domeEthernet.gatewayIP()));
	serializeJson(controllerResp, sResp);
	DBPrintln("sResp : " + sResp);

	res.set("Content-Type", "application/json");
	res.write((uint8_t*)(sResp.c_str()),sResp.length());
}

void stepPerOpenValue(Request &req, Response &res)
{
	JsonDocument controllerResp;
	String sResp;

	if(req.method() == Request::PUT) {
		JsonDocument FormData;
		formDataToJson(req, FormData);
		if(FormData.size()==0){
		}
		else {
			if(FormData["value"].is<long>()) {
				podController->SetStepsPerStroke(FormData["value"]);
			}
		}
	}

	controllerResp["value"] = podController->GetStepsPerStroke();
	serializeJson(controllerResp, sResp);
	DBPrintln("sResp : " + sResp);

	res.set("Content-Type", "application/json");
	res.write((uint8_t*)(sResp.c_str()),sResp.length());
}

void roofSpeedValue(Request &req, Response &res)
{
	JsonDocument controllerResp;
	String sResp;

	if(req.method() == Request::PUT) {
		JsonDocument FormData;
		formDataToJson(req, FormData);
		if(FormData.size()==0){
		}
		else {
			if(FormData["value"].is<long>()) {
				podController->SetMaxSpeed(FormData["value"]);
			}
		}
	}

	controllerResp["value"] = podController->GetMaxSpeed();
	serializeJson(controllerResp, sResp);
	DBPrintln("sResp : " + sResp);

	res.set("Content-Type", "application/json");
	res.write((uint8_t*)(sResp.c_str()),sResp.length());
}

void roofAccelerationValue(Request &req, Response &res)
{
	JsonDocument controllerResp;
	String sResp;

	if(req.method() == Request::PUT) {
		JsonDocument FormData;
		formDataToJson(req, FormData);
		if(FormData.size()==0){
		}
		else {
			if(FormData["value"].is<long>()) {
				podController->SetAcceleration(FormData["value"]);
			}
		}
	}

	controllerResp["value"] = podController->GetAcceleration();
	serializeJson(controllerResp, sResp);
	DBPrintln("sResp : " + sResp);

	res.set("Content-Type", "application/json");
	res.write((uint8_t*)(sResp.c_str()),sResp.length());
}


void restoreMotorValues(Request &req, Response &res)
{
	JsonDocument controllerResp;
	String sResp;

	podController->restoreDefaultMotorSettings();
	controllerResp["value"] = "Restored";
	serializeJson(controllerResp, sResp);
	DBPrintln("sResp : " + sResp);

	res.set("Content-Type", "application/json");
	res.write((uint8_t*)(sResp.c_str()),sResp.length());
}


void roofVoltageCutoffValue(Request &req, Response &res)
{
	JsonDocument controllerResp;
	String sResp;

	if(req.method() == Request::PUT) {
		JsonDocument FormData;
		formDataToJson(req, FormData);
		if(FormData.size()==0){
		}
		else {
			if(FormData["value"].is<long>()) {
				podController->SetLowVoltageCutoff(FormData["value"]);
			}
		}
	}

	controllerResp["value"] = podController->GetVoltString();
	serializeJson(controllerResp, sResp);
	DBPrintln("sResp : " + sResp);

	res.set("Content-Type", "application/json");
	res.write((uint8_t*)(sResp.c_str()),sResp.length());
}

#pragma message FIXME
void unsafeAction(Request &req, Response &res)
{
	JsonDocument controllerResp;
	String sResp;

	if(req.method() == Request::PUT) {
		JsonDocument FormData;
		formDataToJson(req, FormData);
		if(FormData.size()==0){
		}
		else {
			if(FormData["value"].is<long>()) {
				// podController->SetConditionsAction(FormData["value"]);
			}
		}
	}

	// controllerResp["value"] = podController->GetConditionsAction();
	serializeJson(controllerResp, sResp);
	DBPrintln("sResp : " + sResp);

	res.set("Content-Type", "application/json");
	res.write((uint8_t*)(sResp.c_str()),sResp.length());
}


void envConditionState(Request &req, Response &res)
{
	JsonDocument controllerResp;
	String sResp;

	controllerResp["value"] = bool(bIsSafe);
	serializeJson(controllerResp, sResp);
	DBPrintln("sResp : " + sResp);

	res.set("Content-Type", "application/json");
	res.write((uint8_t*)(sResp.c_str()),sResp.length());
}


*/

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



AlpacaServer::AlpacaServer(int port)
{
	String sSerialNumber;    // Mac address, uses part of the unique ID

	m_nRestPort = port;
	mRestServer = nullptr;
	m_AlpacaRestServer = nullptr;
	nTransactionID = 0;

	globalPodConfig->getSerialNumber(sSerialNumber);

	PodUuid.seed(sSerialNumber[4],sSerialNumber[5]);
	PodUuid.generate();

	PodPowerUuid.seed(sSerialNumber[4],sSerialNumber[5]+1);
	PodPowerUuid.generate();



}
/*
AlpacaServer::AlpacaServer(IPAddress ipAddress, int port)
{
	String sSerialNumber;    // Mac address, uses part of the unique ID

	m_nRestPort = port;
	m_ipAddress = ipAddress;
	mRestServer = nullptr;
	m_AlpacaRestServer = nullptr;
	nTransactionID = 0;

	globalPodConfig->getSerialNumber(sSerialNumber);

	PodUuid.seed(sSerialNumber[4],sSerialNumber[5]);
	PodUuid.generate();

	PodPowerUuid.seed(sSerialNumber[4],sSerialNumber[5]+1);
	PodPowerUuid.generate();



}
*/
void AlpacaServer::startServer()
{
	mRestServer = new NetworkServer(m_nRestPort);
	m_AlpacaRestServer = new Application();
	DBPrintln("m_AlpacaRestServer starting");
	DBPrintln("m_AlpacaRestServer UUID : " + String(PodUuid.toCharArray()));
	// check if we're connected as client, if not, use AP IP
	mRestServer->begin();
	DBPrintln("m_AlpacaRestServer mapping endpoints");
	m_AlpacaRestServer->use("/", &doSetup);
	m_AlpacaRestServer->use("/setup", &doSetup);
	m_AlpacaRestServer->get("/management/apiversions", &getApiVersion);
	m_AlpacaRestServer->get("/management/v1/configureddevices", &getConfiguredDevice);
	m_AlpacaRestServer->get("/management/v1/description", &getDescription);
	m_AlpacaRestServer->use("/setup/v1/dome/0/setup", &doSetup);
	m_AlpacaRestServer->put("/api/v1/dome/0/action", &doAction);
	m_AlpacaRestServer->put("/api/v1/dome/0/commandblind", &doCommandBlind);
	m_AlpacaRestServer->put("/api/v1/dome/0/commandbool", &doCommandBool);
	m_AlpacaRestServer->put("/api/v1/dome/0/commandstring", &doCommandString);
	m_AlpacaRestServer->get("/api/v1/dome/0/connected", &getConnected);
	m_AlpacaRestServer->put("/api/v1/dome/0/connected", &setConnected);
	// platform 7
	m_AlpacaRestServer->put("/api/v1/dome/0/connect", &domeConnect);
	m_AlpacaRestServer->get("/api/v1/dome/0/connecting", &domeConnecting);
	m_AlpacaRestServer->put("/api/v1/dome/0/disconnect", &domeDisconnect);
	m_AlpacaRestServer->get("/api/v1/dome/0/devicestate", &getDomeState);
	//
	m_AlpacaRestServer->get("/api/v1/dome/0/description", &getDeviceDescription);
	m_AlpacaRestServer->get("/api/v1/dome/0/driverinfo", &getDriverInfo);
	m_AlpacaRestServer->get("/api/v1/dome/0/driverversion", &getDriverVersion);
	m_AlpacaRestServer->get("/api/v1/dome/0/interfaceversion", &getInterfaceVersion);
	m_AlpacaRestServer->get("/api/v1/dome/0/name", &getName);
	m_AlpacaRestServer->get("/api/v1/dome/0/supportedactions", &getSupportedActions);
	m_AlpacaRestServer->get("/api/v1/dome/0/altitude", &getAltitude);
	m_AlpacaRestServer->get("/api/v1/dome/0/athome", &geAtHome);
	m_AlpacaRestServer->get("/api/v1/dome/0/atpark", &geAtPark);
	m_AlpacaRestServer->get("/api/v1/dome/0/azimuth", &getAzimuth);
	m_AlpacaRestServer->get("/api/v1/dome/0/canfindhome", &canfindhome);
	m_AlpacaRestServer->get("/api/v1/dome/0/canpark", &canPark);
	m_AlpacaRestServer->get("/api/v1/dome/0/cansetaltitude", &canSetAltitude);
	m_AlpacaRestServer->get("/api/v1/dome/0/cansetazimuth", &canSetAzimuth);
	m_AlpacaRestServer->get("/api/v1/dome/0/cansetpark", &canSetPark);
	m_AlpacaRestServer->get("/api/v1/dome/0/cansetshutter", &canSetShutter);
	m_AlpacaRestServer->get("/api/v1/dome/0/canslave", &canSlave);
	m_AlpacaRestServer->get("/api/v1/dome/0/cansyncazimuth", &canSyncAzimuth);
	m_AlpacaRestServer->get("/api/v1/dome/0/shutterstatus", &getShutterStatus);
	m_AlpacaRestServer->get("/api/v1/dome/0/slaved", &getSlaved);
	m_AlpacaRestServer->put("/api/v1/dome/0/slaved", &setSlaved);
	m_AlpacaRestServer->get("/api/v1/dome/0/slewing", &getSlewing);
	m_AlpacaRestServer->put("/api/v1/dome/0/abortslew", &doAbort);
	m_AlpacaRestServer->put("/api/v1/dome/0/closeshutter", &doCloseShutter);
	m_AlpacaRestServer->put("/api/v1/dome/0/findhome", &doFindHome);
	m_AlpacaRestServer->put("/api/v1/dome/0/openshutter", &doOpenShutter);
	m_AlpacaRestServer->put("/api/v1/dome/0/park", &doPark);
	m_AlpacaRestServer->put("/api/v1/dome/0/setpark", &setPark);
	m_AlpacaRestServer->put("/api/v1/dome/0/slewtoaltitude", &doAltitudeSlew);
	m_AlpacaRestServer->put("/api/v1/dome/0/slewtoazimuth", &doGoTo);
	m_AlpacaRestServer->put("/api/v1/dome/0/synctoazimuth", &doSyncAzimuth);



	// adding our own endpoints for the settings
/*
	m_AlpacaRestServer->use("/setup/useDHCP", &useDHCPState);
	m_AlpacaRestServer->use("/setup/ipAddress", &ipAddressValue);
	m_AlpacaRestServer->use("/setup/subnetMask", &subnetMaskValue);
	m_AlpacaRestServer->use("/setup/ipGateway", &ipGatewayValue);
	m_AlpacaRestServer->use("/setup/podShutterCalibrate", &podCalibrateAction);
	m_AlpacaRestServer->get("/setup/envCondition", &envConditionState);
	m_AlpacaRestServer->put("/setup/podOpen", &podOpen);
	m_AlpacaRestServer->put("/setup/podClose", &podClose);
	m_AlpacaRestServer->put("/setup/podState", &podState);
	*/

	m_AlpacaRestServer->get("/setup/serialNumber", &getSerialNumber);
	DBPrintln("m_AlpacaRestServer started");
}


void AlpacaServer::checkForRequest()
{
	// process incoming connections one at a time
	NetworkClient client = mRestServer->accept();
	if (client.connected()) {
		m_AlpacaRestServer->process(&client);
		client.stop();
		nTransactionID++;
  }
}
