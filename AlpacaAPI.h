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
#define POD_INTERFACE_VERSION 3
#define SWITCH_INTERFACE_VERSION 3

#define UDP_PACKET_MAX_SIZE 16

volatile bool bParked = false;

enum AlpacaShutterStates {A_OPEN=0, A_CLOSED, A_OPENING, A_CLOSING, A_ERROR};

#define NB_MAX_SWITCH 5
enum AlpacaSwicthId {A_DC1=0, A_DC2, A_PWM1, A_PWM2, A_USB_C};

uint32_t nTransactionID;
UUID PodUuid, PodPowerUuid;
String sAlpacaDiscovery = "alpacadiscovery1";
volatile bool bAlpacaPodConnected = false;
volatile bool bAlpacaSwitchConnected = false;

class AlpacaDiscoveryServer
{
public:
	AlpacaDiscoveryServer(int port=ALPACA_DISCOVERY_PORT);
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
	AlpacaServer(int port=ALPACA_SERVER_PORT);
	void startServer();
	void checkForRequest();
private :
	NetworkServer *mRestServer;
	Application  *m_AlpacaRestServer;
	int m_nRestPort;
};

AlpacaDiscoveryServer *pod_AlpacaDiscoveryServer;
AlpacaServer *pod_AlpacaServer;

// ALPACA discovery server

AlpacaDiscoveryServer::AlpacaDiscoveryServer( int port)
{
	m_UDPPort = port;
	discoveryServer = nullptr;
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

		DBPrintln(String(__func__) + " : name :'" + String(sName) + "' with value : '" + String(sValue) + "'");

		if(isDigit(value[0])) {
			if(sValue.indexOf('.') == -1) {
				// int
				FormData[sName] = sValue.toInt();
			} else {
				// check if it could be an IP (more than one dot)
				int dotCount = 0;
				for(char c : sValue) if(c == '.') dotCount++;
				if(dotCount > 1) {
					// IP address or similar — treat as string
					FormData[sName] = sValue;
				} else {
					// float
					FormData[sName] = sValue.toFloat();
				}
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

	DBPrintln("[ **********" + String(__func__) + "********** ]");
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

	DBPrintln("[ **********" + String(__func__) + "********** ]");
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

void AlpacaError_x400(JsonDocument AlpacaResp, Response &res)
{
	String sResp;
	AlpacaResp["ErrorNumber"] = 0x400;
	AlpacaResp["ErrorMessage"] = "Not Implemented";
	AlpacaResp["Value"] = false;
	serializeJson(AlpacaResp, sResp);
	res.set("Content-Type", "application/json");
	res.write((uint8_t*)(sResp.c_str()),sResp.length());
}

void AlpacaError_x401(JsonDocument &AlpacaResp, Response &res, String errMsg="Invalid parameters")
{
	String sResp;
	AlpacaResp["ErrorNumber"] = 0x401;
	AlpacaResp["ErrorMessage"] = errMsg;
	serializeJson(AlpacaResp, sResp);
	res.set("Content-Type", "application/json");
	res.write((uint8_t*)(sResp.c_str()),sResp.length());
}


void getApiVersion(Request &req, Response &res)
{
	JsonDocument AlpacaResp;
	JsonDocument FormData;
	bool bParamsOk = false;
	String sResp;

	DBPrintln("[ **********" + String(__func__) + "********** ]");
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

	DBPrintln("[ **********" + String(__func__) + "********** ]");
	bParamsOk = getIDs(req, AlpacaResp, FormData);
	res.set("Content-Type", "application/json");
	AlpacaResp["Value"]["ServerName"]= "Pulsar Imaging Pod";
	AlpacaResp["Value"]["Manufacturer"]= "Pulsar Observatories";
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

	DBPrintln("[ **********" + String(__func__) + "********** ]");
	bParamsOk = getIDs(req, AlpacaResp, FormData);
	res.set("Content-Type", "application/json");
	AlpacaResp["Value"][0] ["DeviceName"]= "Pulsar-Imaging-Pod";
	AlpacaResp["Value"][0] ["DeviceType"]= "dome";
	AlpacaResp["Value"][0] ["DeviceNumber"]= 0;
	AlpacaResp["Value"][0] ["UniqueID"]= PodUuid;

	AlpacaResp["Value"][1] ["DeviceName"]= "Pulsar-Imaging-Pod-Power";
	AlpacaResp["Value"][1] ["DeviceType"]= "switch";
	AlpacaResp["Value"][1] ["DeviceNumber"]= 0;
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

	DBPrintln("[ **********" + String(__func__) + "********** ]");
	bParamsOk = getIDs(req, AlpacaResp, FormData);
	res.set("Content-Type", "application/json");
	if(!bParamsOk){
		AlpacaError_x401(AlpacaResp, res);
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

	DBPrintln("[ **********" + String(__func__) + "********** ]");
	bParamsOk = getIDs(req, AlpacaResp, FormData);
	res.set("Content-Type", "application/json");
	if(!bParamsOk){
		AlpacaError_x401(AlpacaResp, res);
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

	DBPrintln("[ **********" + String(__func__) + "********** ]");
	bParamsOk = getIDs(req, AlpacaResp, FormData);
	res.set("Content-Type", "application/json");
	if(!bParamsOk){
		AlpacaError_x401(AlpacaResp, res);
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

	DBPrintln("[ **********" + String(__func__) + "********** ]");
	bParamsOk = getIDs(req, AlpacaResp, FormData);
	res.set("Content-Type", "application/json");
	if(!bParamsOk){
		AlpacaError_x401(AlpacaResp, res);
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

	DBPrintln("[ **********" + String(__func__) + "********** ]");
	bParamsOk = getIDs(req, AlpacaResp, FormData);
	res.set("Content-Type", "application/json");
	AlpacaResp["ErrorNumber"] = 0;
	AlpacaResp["ErrorMessage"] = "";
	AlpacaResp["Value"] = bAlpacaPodConnected;
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

	DBPrintln("[ **********" + String(__func__) + "********** ]");
	bParamsOk = getIDs(req, AlpacaResp, FormData);
	res.set("Content-Type", "application/json");
	if(!bParamsOk){
		AlpacaError_x401(AlpacaResp, res);
		return;
	}

	if(!FormData["connected"].is<bool>()) {
		AlpacaError_x401(AlpacaResp, res, "Invalid parameters, missing 'Connected'");
		return;
	}

	bAlpacaPodConnected = FormData["connected"];
	DBPrintln("bAlpacaPodConnected : " + (bAlpacaPodConnected?String("true"):String("false")));
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

	DBPrintln("[ **********" + String(__func__) + "********** ]");
	bParamsOk = getIDs(req, AlpacaResp, FormData);
	res.set("Content-Type", "application/json");

	if(!bParamsOk){
		AlpacaError_x401(AlpacaResp, res);
		return;
	}

	bAlpacaPodConnected = true;
	DBPrintln("bAlpacaPodConnected : " + (bAlpacaPodConnected?String("true"):String("false")));

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

	DBPrintln("[ **********" + String(__func__) + "********** ]");
	bParamsOk = getIDs(req, AlpacaResp, FormData);
	res.set("Content-Type", "application/json");

	if(!bParamsOk){
		AlpacaError_x401(AlpacaResp, res);
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

	DBPrintln("[ **********" + String(__func__) + "********** ]");
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

	DBPrintln("[ **********" + String(__func__) + "********** ]");
	bParamsOk = getIDs(req, AlpacaResp, FormData);
	res.set("Content-Type", "application/json");

	if(!bParamsOk){
		AlpacaError_x401(AlpacaResp, res);
		return;
	}

	bAlpacaPodConnected = false;
	DBPrintln("bAlpacaPodConnected : " + (bAlpacaPodConnected?String("true"):String("false")));

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
	DBPrintln("[ **********" + String(__func__) + "********** ]");
	bParamsOk = getIDs(req, AlpacaResp, FormData);
	res.set("Content-Type", "application/json");
	AlpacaResp["ErrorNumber"] = 0;
	AlpacaResp["ErrorMessage"] = "";
	AlpacaResp["Value"]= "Pulsar Imaging Pod controller";
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
	DBPrintln("[ **********" + String(__func__) + "********** ]");
	bParamsOk = getIDs(req, AlpacaResp, FormData);
	res.set("Content-Type", "application/json");
	AlpacaResp["ErrorNumber"] = 0;
	AlpacaResp["ErrorMessage"] = "";
	AlpacaResp["Value"]= "Pulsar Imaging Pod controller";
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
	DBPrintln("[ **********" + String(__func__) + "********** ]");
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
	DBPrintln("[ **********" + String(__func__) + "********** ]");
	bParamsOk = getIDs(req, AlpacaResp, FormData);
	res.set("Content-Type", "application/json");
	AlpacaResp["ErrorNumber"] = 0;
	AlpacaResp["ErrorMessage"] = "";
	AlpacaResp["Value"]= POD_INTERFACE_VERSION;
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
	DBPrintln("[ **********" + String(__func__) + "********** ]");
	bParamsOk = getIDs(req, AlpacaResp, FormData);
	res.set("Content-Type", "application/json");
	AlpacaResp["ErrorNumber"] = 0;
	AlpacaResp["ErrorMessage"] = "";
	AlpacaResp["Value"]= "Pulsar Imaging Pod controller";
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

	DBPrintln("[ **********" + String(__func__) + "********** ]");
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
	DBPrintln("[ **********" + String(__func__) + "********** ]");
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
	DBPrintln("[ **********" + String(__func__) + "********** ]");
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
	DBPrintln("[ **********" + String(__func__) + "********** ]");
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
	DBPrintln("[ **********" + String(__func__) + "********** ]");
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
	DBPrintln("[ **********" + String(__func__) + "********** ]");
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
	DBPrintln("[ **********" + String(__func__) + "********** ]");
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
	DBPrintln("[ **********" + String(__func__) + "********** ]");
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
	DBPrintln("[ **********" + String(__func__) + "********** ]");
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
	DBPrintln("[ **********" + String(__func__) + "********** ]");
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
	DBPrintln("[ **********" + String(__func__) + "********** ]");
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
	DBPrintln("[ **********" + String(__func__) + "********** ]");
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
	DBPrintln("[ **********" + String(__func__) + "********** ]");
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

	DBPrintln("[ **********" + String(__func__) + "********** ]");
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
	DBPrintln("[ **********" + String(__func__) + "********** ]");
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
	DBPrintln("[ **********" + String(__func__) + "********** ]");
	bParamsOk = getIDs(req, AlpacaResp, FormData);
	AlpacaError_x400(AlpacaResp, res);

}

void getSlewing(Request &req, Response &res)
{
	JsonDocument AlpacaResp;
	JsonDocument FormData;
	bool bParamsOk = false;
	String sResp;
	DBPrintln("[ **********" + String(__func__) + "********** ]");
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
	DBPrintln("[ **********" + String(__func__) + "********** ]");
	bParamsOk = getIDs(req, AlpacaResp, FormData);
	res.set("Content-Type", "application/json");
	if(!bParamsOk){
		AlpacaError_x401(AlpacaResp, res);
		return;
	}

	AlpacaResp["ErrorNumber"] = 0;
	AlpacaResp["ErrorMessage"] = "";
	podController->Stop();

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

	DBPrintln("[ **********" + String(__func__) + "********** ]");
	bParamsOk = getIDs(req, AlpacaResp, FormData);
	res.set("Content-Type", "application/json");
	if(!bParamsOk){
		AlpacaError_x401(AlpacaResp, res);
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
	DBPrintln("[ **********" + String(__func__) + "********** ]");
	bParamsOk = getIDs(req, AlpacaResp, FormData);
	res.set("Content-Type", "application/json");
	if(!bParamsOk){
		AlpacaError_x401(AlpacaResp, res);
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

	DBPrintln("[ **********" + String(__func__) + "********** ]");
	bParamsOk = getIDs(req, AlpacaResp, FormData);
	res.set("Content-Type", "application/json");
	if(!bParamsOk){
		AlpacaError_x401(AlpacaResp, res);
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
	DBPrintln("[ **********" + String(__func__) + "********** ]");
	bParamsOk = getIDs(req, AlpacaResp, FormData);
	res.set("Content-Type", "application/json");
	if(!bParamsOk){
		AlpacaError_x401(AlpacaResp, res);
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
	DBPrintln("[ **********" + String(__func__) + "********** ]");
	bParamsOk = getIDs(req, AlpacaResp, FormData);
	res.set("Content-Type", "application/json");
	if(!bParamsOk){
		AlpacaError_x401(AlpacaResp, res);
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
	DBPrintln("[ **********" + String(__func__) + "********** ]");
	bParamsOk = getIDs(req, AlpacaResp, FormData);
	if(!bParamsOk){
		AlpacaError_x401(AlpacaResp, res);
		return;
	}

	if(!FormData["altitude"].is<double>()) {
		AlpacaError_x401(AlpacaResp, res, "Invalid value");
		return;
	}

	res.set("Content-Type", "application/json");
	AlpacaError_x400(AlpacaResp, res);
}

void doGoTo(Request &req, Response &res)
{
	JsonDocument AlpacaResp;
	JsonDocument FormData;
	bool bParamsOk = false;
	String sResp;
	double dNewPos;
	DBPrintln("[ **********" + String(__func__) + "********** ]");
	bParamsOk = getIDs(req, AlpacaResp, FormData);
	res.set("Content-Type", "application/json");
	if(!bParamsOk){
		AlpacaError_x401(AlpacaResp, res);
		return;
	}


	if(!FormData["azimuth"].is<double>()) {
		AlpacaError_x401(AlpacaResp, res);
		return;
	}

	dNewPos = FormData["azimuth"];
	if(dNewPos < 0 || dNewPos>360) {
		AlpacaError_x401(AlpacaResp, res, "Invalid azimuth");
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
	DBPrintln("[ **********" + String(__func__) + "********** ]");
	bParamsOk = getIDs(req, AlpacaResp, FormData);
	res.set("Content-Type", "application/json");
	if(!bParamsOk){
		AlpacaError_x401(AlpacaResp, res);
		return;
	}

	if(!FormData["azimuth"].is<double>()) {
		AlpacaError_x401(AlpacaResp, res, "Invalid azimuth");
		return;
	}

	dNewPos = FormData["azimuth"];
	if(dNewPos<0 || dNewPos > 360) {
		AlpacaError_x401(AlpacaResp, res, "Invalid Azimuth");
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
	DBPrintln("[ **********" + String(__func__) + "********** ]");
	bParamsOk = getIDs(req, AlpacaResp, FormData);
	sHTML = "<!DOCTYPE html>\n<html>\n";
	sHTML += "<head>";
	sHTML += "<title>Pulsar Imaging POD Setup</title>\n";
	sHTML += "</head>\n";
	sHTML += "<body>\n";
	sHTML += "<H1>Pulsar Imaging POD Setup</H1>\n";

	sHTML += "</body>\n</html>\n";
	res.print(sHTML);
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

void useDHCPState(Request &req, Response &res)
{
	JsonDocument controllerResp;
	String sResp;
	bool bUseDhcp = false;

	if(req.method() == Request::PUT) {
		JsonDocument FormData;
		formDataToJson(req, FormData);
		if(FormData.size()==0){
			AlpacaError_x401(controllerResp, res);
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
			AlpacaError_x401(controllerResp, res);
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
			AlpacaError_x401(controllerResp, res);
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
			AlpacaError_x401(controllerResp, res);
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
			AlpacaError_x401(controllerResp, res);
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
		AlpacaError_x401(controllerResp, res, "POD can't read environment sensor.");
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
			AlpacaError_x401(controllerResp, res);
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
			AlpacaError_x401(controllerResp, res);
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
			AlpacaError_x401(controllerResp, res);
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
			AlpacaError_x401(controllerResp, res);
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
			AlpacaError_x401(controllerResp, res);
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


// Switch Alpaca interface
void getSwitchConnected(Request &req, Response &res)
{
	JsonDocument AlpacaResp;
	JsonDocument FormData;
	bool bParamsOk = false;
	String sResp;

	DBPrintln("[ **********" + String(__func__) + "********** ]");
	bParamsOk = getIDs(req, AlpacaResp, FormData);
	res.set("Content-Type", "application/json");
	AlpacaResp["ErrorNumber"] = 0;
	AlpacaResp["ErrorMessage"] = "";
	AlpacaResp["Value"] = bAlpacaSwitchConnected;
	serializeJson(AlpacaResp, sResp);
	DBPrintln("sResp : " + sResp);
	res.write((uint8_t*)(sResp.c_str()),sResp.length());
}

void setSwitchConnected(Request &req, Response &res)
{
	JsonDocument AlpacaResp;
	JsonDocument FormData;
	bool bParamsOk = false;
	String sResp;
	String sClientId;
	String sClientTransactionId;
	String sParameter;
	String sTmp;

	DBPrintln("[ **********" + String(__func__) + "********** ]");
	bParamsOk = getIDs(req, AlpacaResp, FormData);
	res.set("Content-Type", "application/json");
	if(!bParamsOk){
		AlpacaError_x401(AlpacaResp, res);
		return;
	}

	if(!FormData["connected"].is<bool>()) {
		AlpacaError_x401(AlpacaResp, res, "Invalid parameters, missing 'Connected'");
		return;
	}

	bAlpacaSwitchConnected = FormData["connected"];
	DBPrintln("bAlpacaSwitchConnected : " + (bAlpacaSwitchConnected?String("true"):String("false")));
	AlpacaResp["ErrorNumber"] = 0;
	AlpacaResp["ErrorMessage"] = "";
	serializeJson(AlpacaResp, sResp);
	DBPrintln("sResp : " + sResp);
	res.write((uint8_t*)(sResp.c_str()),sResp.length());
}

void switchConnect(Request &req, Response &res)
{
	JsonDocument AlpacaResp;
	JsonDocument FormData;
	bool bParamsOk = false;
	String sResp;
	String sClientId;
	String sClientTransactionId;
	String sParameter;
	String sTmp;

	DBPrintln("[ **********" + String(__func__) + "********** ]");
	bParamsOk = getIDs(req, AlpacaResp, FormData);
	res.set("Content-Type", "application/json");

	if(!bParamsOk){
		AlpacaError_x401(AlpacaResp, res);
		return;
	}

	bAlpacaSwitchConnected = true;
	DBPrintln("bAlpacaSwitchConnected : " + (bAlpacaSwitchConnected?String("true"):String("false")));

	AlpacaResp["ErrorNumber"] = 0;
	AlpacaResp["ErrorMessage"] = "";
	serializeJson(AlpacaResp, sResp);
	DBPrintln("sResp : " + sResp);

	res.write((uint8_t*)(sResp.c_str()),sResp.length());
}

void switchConnecting(Request &req, Response &res)
{
	JsonDocument AlpacaResp;
	JsonDocument FormData;
	bool bParamsOk = false;
	String sResp;
	String sClientId;
	String sClientTransactionId;
	String sParameter;
	String sTmp;

	DBPrintln("[ **********" + String(__func__) + "********** ]");
	bParamsOk = getIDs(req, AlpacaResp, FormData);
	res.set("Content-Type", "application/json");

	if(!bParamsOk){
		AlpacaError_x401(AlpacaResp, res);
		return;
	}

	AlpacaResp["ErrorNumber"] = 0;
	AlpacaResp["ErrorMessage"] = "";
	AlpacaResp["Value"] = false; // it's already connected
	serializeJson(AlpacaResp, sResp);
	DBPrintln("sResp : " + sResp);

	res.write((uint8_t*)(sResp.c_str()),sResp.length());
}

void getSwitchDeviceState(Request &req, Response &res)
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
	int nPercent;
	bool bPortOn;
	DBPrintln("[ **********" + String(__func__) + "********** ]");
	bParamsOk = getIDs(req, AlpacaResp, FormData);

	res.set("Content-Type", "application/json");
	AlpacaResp["ErrorNumber"] = 0;
	AlpacaResp["ErrorMessage"] = "";

	// add states to response
	podPowerController->getPortState(DC1, bPortOn);
	jsTmp["Name"] = "GetSwitch0";
	jsTmp["Value"] = (bPortOn?"On":"Off");
	AlpacaResp["Value"].add(jsTmp);
	jsTmp.clear();

	jsTmp["Name"] = "GetSwitchValue0";
	jsTmp["Value"] = (bPortOn?12:0);
	AlpacaResp["Value"].add(jsTmp);
	jsTmp.clear();


	podPowerController->getPortState(DC2, bPortOn);
	jsTmp["Name"] = "GetSwitch1";
	jsTmp["Value"] = (bPortOn?"On":"Off");
	AlpacaResp["Value"].add(jsTmp);
	jsTmp.clear();

	jsTmp["Name"] = "GetSwitchValue1";
	jsTmp["Value"] = (bPortOn?12:0);
	AlpacaResp["Value"].add(jsTmp);
	jsTmp.clear();

	podPowerController->getPwmPortState(PWM1, nPercent);
	jsTmp["Name"] = "GetSwitch2";
	jsTmp["Value"] = (nPercent>0?"On":"Off");
	AlpacaResp["Value"].add(jsTmp);
	jsTmp.clear();

	jsTmp["Name"] = "GetSwitchValue2";
	jsTmp["Value"] = nPercent;
	AlpacaResp["Value"].add(jsTmp);
	jsTmp.clear();

	podPowerController->getPwmPortState(PWM2, nPercent);
	jsTmp["Name"] = "GetSwitch3";
	jsTmp["Value"] = (nPercent>0?"On":"Off");
	AlpacaResp["Value"].add(jsTmp);
	jsTmp.clear();

	jsTmp["Name"] = "GetSwitchValue3";
	jsTmp["Value"] = nPercent;
	AlpacaResp["Value"].add(jsTmp);
	jsTmp.clear();

	podPowerController->getPortState(USB_C, bPortOn);
	jsTmp["Name"] = "GetSwitch4";
	jsTmp["Value"] = (bPortOn?"On":"Off");
	AlpacaResp["Value"].add(jsTmp);
	jsTmp.clear();

	jsTmp["Name"] = "GetSwitchValue1";
	jsTmp["Value"] = (bPortOn?5:0);
	AlpacaResp["Value"].add(jsTmp);
	jsTmp.clear();

	serializeJson(AlpacaResp, sResp);
	DBPrintln("sResp : " + sResp);

	res.write((uint8_t*)(sResp.c_str()),sResp.length());
}

void switchDisconnect(Request &req, Response &res)
{
	JsonDocument AlpacaResp;
	JsonDocument FormData;
	bool bParamsOk = false;
	String sResp;
	String sClientId;
	String sClientTransactionId;
	String sParameter;
	String sTmp;

	DBPrintln("[ **********" + String(__func__) + "********** ]");
	bParamsOk = getIDs(req, AlpacaResp, FormData);
	res.set("Content-Type", "application/json");

	if(!bParamsOk){
		AlpacaError_x401(AlpacaResp, res);
		return;
	}

	bAlpacaSwitchConnected = false;
	DBPrintln("bAlpacaSwitchConnected : " + (bAlpacaSwitchConnected?String("true"):String("false")));

	AlpacaResp["ErrorNumber"] = 0;
	AlpacaResp["ErrorMessage"] = "";
	serializeJson(AlpacaResp, sResp);
	DBPrintln("sResp : " + sResp);

	res.write((uint8_t*)(sResp.c_str()),sResp.length());
}

void getSwitchDeviceDescription(Request &req, Response &res)
{
	JsonDocument AlpacaResp;
	JsonDocument FormData;
	bool bParamsOk = false;
	String sResp;
	DBPrintln("[ **********" + String(__func__) + "********** ]");
	bParamsOk = getIDs(req, AlpacaResp, FormData);
	res.set("Content-Type", "application/json");
	AlpacaResp["ErrorNumber"] = 0;
	AlpacaResp["ErrorMessage"] = "";
	AlpacaResp["Value"]= "Pulsar Imaging POD power controller";
	serializeJson(AlpacaResp, sResp);
	DBPrintln("sResp : " + sResp);
	res.write((uint8_t*)(sResp.c_str()),sResp.length());
}

void getSwitchDriverInfo(Request &req, Response &res)
{
	JsonDocument AlpacaResp;
	JsonDocument FormData;
	bool bParamsOk = false;
	String sResp;

	DBPrintln("[ **********" + String(__func__) + "********** ]");
	bParamsOk = getIDs(req, AlpacaResp, FormData);
	res.set("Content-Type", "application/json");
	AlpacaResp["ErrorNumber"] = 0;
	AlpacaResp["ErrorMessage"] = "";
	AlpacaResp["Value"]= "Pulsar Pod power ports";
	serializeJson(AlpacaResp, sResp);
	DBPrintln("sResp : " + sResp);
	res.write((uint8_t*)(sResp.c_str()),sResp.length());
}

void getSwitchInterfaceVersion(Request &req, Response &res)
{
	JsonDocument AlpacaResp;
	JsonDocument FormData;
	bool bParamsOk = false;
	String sResp;

	DBPrintln("[ **********" + String(__func__) + "********** ]");
	bParamsOk = getIDs(req, AlpacaResp, FormData);
	res.set("Content-Type", "application/json");
	AlpacaResp["ErrorNumber"] = 0;
	AlpacaResp["ErrorMessage"] = "";
	AlpacaResp["Value"]= SWITCH_INTERFACE_VERSION;
	serializeJson(AlpacaResp, sResp);
	DBPrintln("sResp : " + sResp);
	res.write((uint8_t*)(sResp.c_str()),sResp.length());
}

void getSwitchDeviceName(Request &req, Response &res)
{
	JsonDocument AlpacaResp;
	JsonDocument FormData;
	bool bParamsOk = false;
	String sResp;

	DBPrintln("[ **********" + String(__func__) + "********** ]");
	bParamsOk = getIDs(req, AlpacaResp, FormData);
	res.set("Content-Type", "application/json");
	AlpacaResp["ErrorNumber"] = 0;
	AlpacaResp["ErrorMessage"] = "";
	AlpacaResp["Value"]= "Pulsar Imaging POD power controller";
	serializeJson(AlpacaResp, sResp);
	DBPrintln("sResp : " + sResp);
	res.write((uint8_t*)(sResp.c_str()),sResp.length());
}

void maxSwitch(Request &req, Response &res)
{
	JsonDocument AlpacaResp;
	JsonDocument FormData;
	bool bParamsOk = false;
	podStates nState;
	String sResp;

	DBPrintln("[ **********" + String(__func__) + "********** ]");
	bParamsOk = getIDs(req, AlpacaResp, FormData);
	res.set("Content-Type", "application/json");
	AlpacaResp["ErrorNumber"] = 0;
	AlpacaResp["ErrorMessage"] = "";

	AlpacaResp["Value"] = NB_MAX_SWITCH;
	serializeJson(AlpacaResp, sResp);
	DBPrintln("sResp : " + sResp);
	res.write((uint8_t*)(sResp.c_str()),sResp.length());
}

void switchCanaSync(Request &req, Response &res)
{
	JsonDocument AlpacaResp;
	JsonDocument FormData;
	bool bParamsOk = false;
	podStates nState;
	String sResp;

	DBPrintln("[ **********" + String(__func__) + "********** ]");
	bParamsOk = getIDs(req, AlpacaResp, FormData);
	res.set("Content-Type", "application/json");
	AlpacaResp["ErrorNumber"] = 0;
	AlpacaResp["ErrorMessage"] = "";

	AlpacaResp["Value"] = true; // not really but it's so fast it's going to be the same.

	serializeJson(AlpacaResp, sResp);
	DBPrintln("sResp : " + sResp);
	res.write((uint8_t*)(sResp.c_str()),sResp.length());
}

void switchCanWrite(Request &req, Response &res)
{
	JsonDocument AlpacaResp;
	JsonDocument FormData;
	bool bParamsOk = false;
	podStates nState;
	String sResp;

	DBPrintln("[ **********" + String(__func__) + "********** ]");
	bParamsOk = getIDs(req, AlpacaResp, FormData);
	res.set("Content-Type", "application/json");
	AlpacaResp["ErrorNumber"] = 0;
	AlpacaResp["ErrorMessage"] = "";

	AlpacaResp["Value"] = true;

	serializeJson(AlpacaResp, sResp);
	DBPrintln("sResp : " + sResp);
	res.write((uint8_t*)(sResp.c_str()),sResp.length());
}

void getSwitch(Request &req, Response &res)
{
	JsonDocument AlpacaResp;
	JsonDocument FormData;
	bool bParamsOk = false;
	podStates nState;
	String sResp;
	int switchId = -1;
	bool bOn = false;
	int nPercent = 0;
	std::vector<std::vector<String>> svParameters;

	DBPrintln("[ **********" + String(__func__) + "********** ]");
	bParamsOk = getIDs(req, AlpacaResp, FormData);
	res.set("Content-Type", "application/json");
	AlpacaResp["ErrorNumber"] = 0;
	AlpacaResp["ErrorMessage"] = "";

	getQueryGetVariables(String(req.query()), svParameters);
	for( std::vector<String> &svParamEntry : svParameters ) {
		if(svParamEntry.at(0).equals("id")) {
			switchId = svParamEntry.at(1).toInt();
		}
	}

	if(switchId<0 || switchId >= NB_MAX_SWITCH) {
		AlpacaError_x401(AlpacaResp, res);
		return;
	}

	switch(switchId) {
		case 0:
			if(podPowerController)
				podPowerController->getPortState(DC1,bOn);
			AlpacaResp["Value"] = bOn;
			break;
		case 1:
			if(podPowerController)
				podPowerController->getPortState(DC2,bOn);
			AlpacaResp["Value"] = bOn;
			break;
		case 2:
			if(podPowerController)
				podPowerController->getPwmPortState(PWM1,nPercent);
			AlpacaResp["Value"] = (nPercent?true:false);
			break;
		case 3:
			if(podPowerController)
				podPowerController->getPwmPortState(PWM2,nPercent);
			AlpacaResp["Value"] = (nPercent?true:false);
			break;
		case 4:
			if(podPowerController)
				podPowerController->getPortState(USB_C,bOn);
			AlpacaResp["Value"] = bOn;
			break;
		default:
			AlpacaError_x401(AlpacaResp, res);
			return;
	}

	serializeJson(AlpacaResp, sResp);
	DBPrintln("sResp : " + sResp);
	res.write((uint8_t*)(sResp.c_str()),sResp.length());
}

void getSwitchDescription(Request &req, Response &res)
{
	JsonDocument AlpacaResp;
	JsonDocument FormData;
	bool bParamsOk = false;
	podStates nState;
	String sResp;
	int switchId = -1;
	std::vector<std::vector<String>> svParameters;

	DBPrintln("[ **********" + String(__func__) + "********** ]");
	bParamsOk = getIDs(req, AlpacaResp, FormData);
	res.set("Content-Type", "application/json");
	AlpacaResp["ErrorNumber"] = 0;
	AlpacaResp["ErrorMessage"] = "";

	getQueryGetVariables(String(req.query()), svParameters);
	for( std::vector<String> &svParamEntry : svParameters ) {
		if(svParamEntry.at(0).equals("id")) {
			switchId = svParamEntry.at(1).toInt();
		}
	}

	if(switchId<0 || switchId >= NB_MAX_SWITCH) {
		AlpacaError_x401(AlpacaResp, res);
		return;
	}

	switch(switchId) {
		case 0:
			AlpacaResp["Value"] = "DC1 power port";
			break;
		case 1:
			AlpacaResp["Value"] = "DC2 power port";
			break;
		case 2:
			AlpacaResp["Value"] = "PWM1 power port";
			break;
		case 3:
			AlpacaResp["Value"] = "PWM2 power port";
			break;
		case 4:
			AlpacaResp["Value"] = "USB-C power port";
			break;
		default:
			AlpacaError_x401(AlpacaResp, res);
			return;
	}

	serializeJson(AlpacaResp, sResp);
	DBPrintln("sResp : " + sResp);
	res.write((uint8_t*)(sResp.c_str()),sResp.length());
}

void getSwitchName(Request &req, Response &res)
{
	JsonDocument AlpacaResp;
	JsonDocument FormData;
	bool bParamsOk = false;
	podStates nState;
	String sResp;
	int switchId = -1;
	String sName;
	std::vector<std::vector<String>> svParameters;

	DBPrintln("[ **********" + String(__func__) + "********** ]");
	bParamsOk = getIDs(req, AlpacaResp, FormData);
	res.set("Content-Type", "application/json");
	AlpacaResp["ErrorNumber"] = 0;
	AlpacaResp["ErrorMessage"] = "";

	getQueryGetVariables(String(req.query()), svParameters);
	for( std::vector<String> &svParamEntry : svParameters ) {
		if(svParamEntry.at(0).equals("id")) {
			switchId = svParamEntry.at(1).toInt();
		}
	}

	if(switchId<0 || switchId >= NB_MAX_SWITCH) {
		AlpacaError_x401(AlpacaResp, res);
		return;
	}

	switch(switchId) {
		case 0:
			if(globalPodConfig)
				globalPodConfig->getAlpacaPortName(DC1,sName);
			AlpacaResp["Value"] = sName;
			break;
		case 1:
			if(globalPodConfig)
				globalPodConfig->getAlpacaPortName(DC2,sName);
			AlpacaResp["Value"] = sName;
			break;
		case 2:
			if(globalPodConfig)
				globalPodConfig->getAlpacaPortName(PWM1,sName);
			AlpacaResp["Value"] = sName;
			break;
		case 3:
			if(globalPodConfig)
				globalPodConfig->getAlpacaPortName(PWM2,sName);
			AlpacaResp["Value"] = sName;
			break;
		case 4:
			if(globalPodConfig)
				globalPodConfig->getAlpacaPortName(USB_C,sName);
			AlpacaResp["Value"] = sName;
			break;
		default:
			AlpacaError_x401(AlpacaResp, res);
			return;
	}

	serializeJson(AlpacaResp, sResp);
	DBPrintln("sResp : " + sResp);
	res.write((uint8_t*)(sResp.c_str()),sResp.length());
}

void getSwitchValue(Request &req, Response &res)
{
	JsonDocument AlpacaResp;
	JsonDocument FormData;
	bool bParamsOk = false;
	podStates nState;
	String sResp;
	int switchId = -1;
	std::vector<std::vector<String>> svParameters;
	int nPercent;
	bool bOn;

	DBPrintln("[ **********" + String(__func__) + "********** ]");
	bParamsOk = getIDs(req, AlpacaResp, FormData);
	res.set("Content-Type", "application/json");
	AlpacaResp["ErrorNumber"] = 0;
	AlpacaResp["ErrorMessage"] = "";

	getQueryGetVariables(String(req.query()), svParameters);
	for( std::vector<String> &svParamEntry : svParameters ) {
		if(svParamEntry.at(0).equals("id")) {
			switchId = svParamEntry.at(1).toInt();
		}
	}

	if(switchId<0 || switchId >= NB_MAX_SWITCH) {
		AlpacaError_x401(AlpacaResp, res);
		return;
	}

	switch(switchId) {
		case 0:
			if(podPowerController) {
				podPowerController->getPortState(DC1 , bOn);
				AlpacaResp["Value"] = bOn;
			}
			break;
		case 1:
			if(podPowerController) {
				podPowerController->getPortState(DC2 , bOn);
				AlpacaResp["Value"] = bOn;
			}
			break;
		case 2:
			if(podPowerController) {
				podPowerController->getPwmPortState(PWM1 , nPercent);
				AlpacaResp["Value"] = nPercent;
			}
			break;
		case 3:
			if(podPowerController) {
				podPowerController->getPwmPortState(PWM2 , nPercent);
				AlpacaResp["Value"] = nPercent;
			}
			break;
		case 4:
			if(podPowerController) {
				podPowerController->getPortState(USB_C , bOn);
				AlpacaResp["Value"] = bOn;
			}
			break;
		default:
			AlpacaError_x401(AlpacaResp, res);
			return;
	}

	serializeJson(AlpacaResp, sResp);
	DBPrintln("sResp : " + sResp);
	res.write((uint8_t*)(sResp.c_str()),sResp.length());
}

void minSwitchValue(Request &req, Response &res)
{
	JsonDocument AlpacaResp;
	JsonDocument FormData;
	bool bParamsOk = false;
	podStates nState;
	String sResp;
	int switchId = -1;
	std::vector<std::vector<String>> svParameters;

	DBPrintln("[ **********" + String(__func__) + "********** ]");
	bParamsOk = getIDs(req, AlpacaResp, FormData);
	res.set("Content-Type", "application/json");
	AlpacaResp["ErrorNumber"] = 0;
	AlpacaResp["ErrorMessage"] = "";

	getQueryGetVariables(String(req.query()), svParameters);
	for( std::vector<String> &svParamEntry : svParameters ) {
		if(svParamEntry.at(0).equals("id")) {
			switchId = svParamEntry.at(1).toInt();
		}
	}

	if(switchId<0 || switchId >= NB_MAX_SWITCH) {
		AlpacaError_x401(AlpacaResp, res);
		return;
	}

	switch(switchId) {
		case 0:
			AlpacaResp["Value"] = 0.0f;
			break;
		case 1:
			AlpacaResp["Value"] = 0.0f;
			break;
		case 2:
			AlpacaResp["Value"] = 0.0f;
			break;
		case 3:
			AlpacaResp["Value"] = 0.0f;
			break;
		case 4:
			AlpacaResp["Value"] = 0.0f;
			break;
		default:
			AlpacaError_x401(AlpacaResp, res);
			return;
	}

	serializeJson(AlpacaResp, sResp);
	DBPrintln("sResp : " + sResp);
	res.write((uint8_t*)(sResp.c_str()),sResp.length());
}

void maxSwitchValue(Request &req, Response &res)
{
	JsonDocument AlpacaResp;
	JsonDocument FormData;
	bool bParamsOk = false;
	podStates nState;
	String sResp;
	int switchId = -1;
	std::vector<std::vector<String>> svParameters;

	DBPrintln("[ **********" + String(__func__) + "********** ]");
	bParamsOk = getIDs(req, AlpacaResp, FormData);
	res.set("Content-Type", "application/json");
	AlpacaResp["ErrorNumber"] = 0;
	AlpacaResp["ErrorMessage"] = "";

	getQueryGetVariables(String(req.query()), svParameters);
	for( std::vector<String> &svParamEntry : svParameters ) {
		if(svParamEntry.at(0).equals("id")) {
			switchId = svParamEntry.at(1).toInt();
		}
	}

	if(switchId<0 || switchId >= NB_MAX_SWITCH) {
		AlpacaError_x401(AlpacaResp, res);
		return;
	}

	switch(switchId) {
		case 0:
			AlpacaResp["Value"] = 1.0f;
			break;
		case 1:
			AlpacaResp["Value"] = 1.0f;
			break;
		case 2:
			AlpacaResp["Value"] = 100.0f;
			break;
		case 3:
			AlpacaResp["Value"] = 100.0f;
			break;
		case 4:
			AlpacaResp["Value"] = 1.0f;
			break;
		default:
			AlpacaError_x401(AlpacaResp, res);
			return;
	}

	serializeJson(AlpacaResp, sResp);
	DBPrintln("sResp : " + sResp);
	res.write((uint8_t*)(sResp.c_str()),sResp.length());
}


void setSwitch(Request &req, Response &res)
{
	JsonDocument AlpacaResp;
	JsonDocument FormData;
	bool bParamsOk = false;
	String sResp;
	int switchId = -1;
	bool bState = false;

	DBPrintln("[ **********" + String(__func__) + "********** ]");
	bParamsOk = getIDs(req, AlpacaResp, FormData);
	res.set("Content-Type", "application/json");
	if(!bParamsOk){
		AlpacaError_x401(AlpacaResp, res);
		return;
	}

	if(!FormData["id"].is<int>()) {
		AlpacaError_x401(AlpacaResp, res);
		return;
	}

	switchId = FormData["id"];
	if(switchId<0 || switchId >= NB_MAX_SWITCH) {
		AlpacaError_x401(AlpacaResp, res);
		return;
	}
	if(!FormData["State"].is<bool>()) {
		AlpacaError_x401(AlpacaResp, res);
		return;
	}

	bState = FormData["State"];
	switch(switchId) {
		case 0:
			if(podPowerController)
				podPowerController->setPortState(DC1, bState);
			break;
		case 1:
			if(podPowerController)
				podPowerController->setPortState(DC2, bState);
			break;
		case 2:
			if(podPowerController)
				podPowerController->setPwmPortState(PWM1,bState?100:0);
			break;
		case 3:
			if(podPowerController)
				podPowerController->setPwmPortState(PWM2,bState?100:0);
			break;
		case 4:
			if(podPowerController)
				podPowerController->setPortState(USB_C, bState);
			break;
		default:
			AlpacaError_x401(AlpacaResp, res);
			return;
	}

	AlpacaResp["ErrorNumber"] = 0;
	AlpacaResp["ErrorMessage"] = "";
	serializeJson(AlpacaResp, sResp);
	DBPrintln("sResp : " + sResp);
	res.write((uint8_t*)(sResp.c_str()),sResp.length());
}

void setSwitchName(Request &req, Response &res)
{
	JsonDocument AlpacaResp;
	JsonDocument FormData;
	bool bParamsOk = false;
	String sResp;
	int switchId = -1;
	String sName;

	DBPrintln("[ **********" + String(__func__) + "********** ]");
	bParamsOk = getIDs(req, AlpacaResp, FormData);
	res.set("Content-Type", "application/json");
	if(!bParamsOk){
		AlpacaError_x401(AlpacaResp, res);
		return;
	}

	if(!FormData["id"].is<int>()) {
		AlpacaError_x401(AlpacaResp, res);
		return;
	}

	switchId = FormData["id"];
	if(switchId<0 || switchId >= NB_MAX_SWITCH) {
		AlpacaError_x401(AlpacaResp, res);
		return;
	}
	if(!FormData["Name"].is<String>()) {
		AlpacaError_x401(AlpacaResp, res);
		return;
	}
	sName = String(FormData["Name"]);
	switch(switchId) {
		case 0:
			if(globalPodConfig)
				globalPodConfig->setAlpacaPortName(DC1, sName);
			break;
		case 1:
			if(globalPodConfig)
				globalPodConfig->setAlpacaPortName(DC2, sName);
			break;
		case 2:
			if(globalPodConfig)
				globalPodConfig->setAlpacaPortName(PWM1, sName);
			break;
		case 3:
			if(globalPodConfig)
				globalPodConfig->setAlpacaPortName(PWM2, sName);
			break;
		case 4:
			if(globalPodConfig)
				globalPodConfig->setAlpacaPortName(USB_C, sName);
			break;
		default:
			AlpacaError_x401(AlpacaResp, res);
			return;
	}

	AlpacaResp["ErrorNumber"] = 0;
	AlpacaResp["ErrorMessage"] = "";
	serializeJson(AlpacaResp, sResp);
	DBPrintln("sResp : " + sResp);
	res.write((uint8_t*)(sResp.c_str()),sResp.length());
}

void setSwitchValue(Request &req, Response &res)
{
	JsonDocument AlpacaResp;
	JsonDocument FormData;
	bool bParamsOk = false;
	String sResp;
	int switchId = -1;
	int nValue = -1;

	DBPrintln("[ **********" + String(__func__) + "********** ]");
	bParamsOk = getIDs(req, AlpacaResp, FormData);
	res.set("Content-Type", "application/json");
	if(!bParamsOk){
		AlpacaError_x401(AlpacaResp, res);
		return;
	}

	if(!FormData["id"].is<int>()) {
		AlpacaError_x401(AlpacaResp, res);
		return;
	}

	switchId = FormData["id"];
	if(switchId<0 || switchId >= NB_MAX_SWITCH) {
		AlpacaError_x401(AlpacaResp, res);
		return;
	}

	if(!FormData["Value"].is<double>()) {
		AlpacaError_x401(AlpacaResp, res);
		return;
	}

	nValue = int(FormData["Value"]);

	switch(switchId) {
		case 0:
			if(nValue < 0 || nValue > 1 ) {
				AlpacaError_x401(AlpacaResp, res);
				return;
			}
			if(podPowerController)
				podPowerController->setPortState(DC1, int(nValue)==1?true:false);
			break;
		case 1:
			if(nValue < 0 || nValue > 1 ) {
				AlpacaError_x401(AlpacaResp, res);
				return;
			}
			if(podPowerController)
				podPowerController->setPortState(DC2, int(nValue)==1?true:false);
			break;
		case 2:
			if(nValue < 0 || nValue > 100 ) {
				AlpacaError_x401(AlpacaResp, res);
				return;
			}
			if(podPowerController)
				podPowerController->setPwmPortState(PWM1PwmChannel,nValue);
			break;
		case 3:
			if(nValue < 0 || nValue > 100 ) {
				AlpacaError_x401(AlpacaResp, res);
				return;
			}
			if(podPowerController)
				podPowerController->setPwmPortState(PWM2PwmChannel,nValue);
			break;
		case 4:
			if(nValue < 0 || nValue > 1 ) {
				AlpacaError_x401(AlpacaResp, res);
				return;
			}
			if(podPowerController)
				podPowerController->setPortState(USB_C, int(nValue)==1?true:false);
			break;
		default:
			AlpacaError_x401(AlpacaResp, res);
			return;
			break;
	}

	AlpacaResp["ErrorNumber"] = 0;
	AlpacaResp["ErrorMessage"] = "";
	serializeJson(AlpacaResp, sResp);
	DBPrintln("sResp : " + sResp);
	res.write((uint8_t*)(sResp.c_str()),sResp.length());
}

void switchStateChangeComplete(Request &req, Response &res)
{
	JsonDocument AlpacaResp;
	JsonDocument FormData;
	bool bParamsOk = false;
	podStates nState;
	String sResp;
	int switchId = -1;
	std::vector<std::vector<String>> svParameters;

	DBPrintln("[ **********" + String(__func__) + "********** ]");
	bParamsOk = getIDs(req, AlpacaResp, FormData);
	res.set("Content-Type", "application/json");
	AlpacaResp["ErrorNumber"] = 0;
	AlpacaResp["ErrorMessage"] = "";

	getQueryGetVariables(String(req.query()), svParameters);
	for( std::vector<String> &svParamEntry : svParameters ) {
		if(svParamEntry.at(0).equals("id")) {
			switchId = svParamEntry.at(1).toInt();
		}
	}

	if(switchId<0 || switchId >= NB_MAX_SWITCH) {
		AlpacaError_x401(AlpacaResp, res);
		return;
	}

	AlpacaResp["Value"] = true;

	serializeJson(AlpacaResp, sResp);
	DBPrintln("sResp : " + sResp);
	res.write((uint8_t*)(sResp.c_str()),sResp.length());
}

void switchStep(Request &req, Response &res)
{
	JsonDocument AlpacaResp;
	JsonDocument FormData;
	bool bParamsOk = false;
	podStates nState;
	String sResp;
	int switchId = -1;
	std::vector<std::vector<String>> svParameters;


	DBPrintln("[ **********" + String(__func__) + "********** ]");
	bParamsOk = getIDs(req, AlpacaResp, FormData);
	res.set("Content-Type", "application/json");
	AlpacaResp["ErrorNumber"] = 0;
	AlpacaResp["ErrorMessage"] = "";

	getQueryGetVariables(String(req.query()), svParameters);
	for( std::vector<String> &svParamEntry : svParameters ) {
		if(svParamEntry.at(0).equals("id")) {
			switchId = svParamEntry.at(1).toInt();
		}
	}

	if(switchId<0 || switchId >= NB_MAX_SWITCH) {
		AlpacaError_x401(AlpacaResp, res);
		return;
	}
	switch(switchId) {
		case 0:
			AlpacaResp["Value"] = 2;
			break;
		case 1:
			AlpacaResp["Value"] = 2;
			break;
		case 2:
			AlpacaResp["Value"] = 100;
			break;
		case 3:
			AlpacaResp["Value"] = 100;
			break;
		case 4:
			AlpacaResp["Value"] = 2;
			break;
		default:
			AlpacaError_x401(AlpacaResp, res);
			return;
	}

	serializeJson(AlpacaResp, sResp);
	DBPrintln("sResp : " + sResp);
	res.write((uint8_t*)(sResp.c_str()),sResp.length());
}


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

	// management
	m_AlpacaRestServer->get("/management/apiversions", &getApiVersion);
	m_AlpacaRestServer->get("/management/v1/configureddevices", &getConfiguredDevice);
	m_AlpacaRestServer->get("/management/v1/description", &getDescription);
	m_AlpacaRestServer->use("/setup/v1/dome/0/setup", &doSetup);

	// dome device 0
	// Common method
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
	// dome specific
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

	// switch device 0
	// Common method
	m_AlpacaRestServer->put("/api/v1/switch/0/action", &doAction);
	m_AlpacaRestServer->put("/api/v1/switch/0/commandblind", &doCommandBlind);
	m_AlpacaRestServer->put("/api/v1/switch/0/commandbool", &doCommandBool);
	m_AlpacaRestServer->put("/api/v1/switch/0/commandstring", &doCommandString);
	m_AlpacaRestServer->get("/api/v1/switch/0/connected", &getSwitchConnected);
	m_AlpacaRestServer->put("/api/v1/switch/0/connected", &setSwitchConnected);
	// platform 7
	m_AlpacaRestServer->put("/api/v1/switch/0/connect", &switchConnect);
	m_AlpacaRestServer->get("/api/v1/switch/0/connecting", &switchConnecting);
	m_AlpacaRestServer->put("/api/v1/switch/0/disconnect", &switchDisconnect);
	m_AlpacaRestServer->get("/api/v1/switch/0/devicestate", &getSwitchDeviceState);
	//
	m_AlpacaRestServer->get("/api/v1/switch/0/description", &getSwitchDeviceDescription);
	m_AlpacaRestServer->get("/api/v1/switch/0/driverinfo", &getSwitchDriverInfo);
	m_AlpacaRestServer->get("/api/v1/switch/0/driverversion", &getDriverVersion);
	m_AlpacaRestServer->get("/api/v1/switch/0/interfaceversion", &getSwitchInterfaceVersion);
	m_AlpacaRestServer->get("/api/v1/switch/0/name", &getSwitchDeviceName);
	m_AlpacaRestServer->get("/api/v1/switch/0/supportedactions", &getSupportedActions);
	//
	m_AlpacaRestServer->get("/api/v1/switch/0/maxswitch", &maxSwitch);
	m_AlpacaRestServer->get("/api/v1/switch/0/canasync", &switchCanaSync);
	m_AlpacaRestServer->get("/api/v1/switch/0/canwrite", &switchCanWrite);
	m_AlpacaRestServer->get("/api/v1/switch/0/getswitch", &getSwitch);
	m_AlpacaRestServer->get("/api/v1/switch/0/getswitchdescription", &getSwitchDescription);
	m_AlpacaRestServer->get("/api/v1/switch/0/getswitchname", &getSwitchName);
	m_AlpacaRestServer->get("/api/v1/switch/0/getswitchvalue", &getSwitchValue);
	m_AlpacaRestServer->get("/api/v1/switch/0/minswitchvalue", &minSwitchValue);
	m_AlpacaRestServer->get("/api/v1/switch/0/maxswitchvalue", &maxSwitchValue);

	m_AlpacaRestServer->put("/api/v1/switch/0/setasync", &setSwitch);
	m_AlpacaRestServer->put("/api/v1/switch/0/setasyncvalue", &setSwitchValue);
	m_AlpacaRestServer->put("/api/v1/switch/0/setswitch", &setSwitch);
	m_AlpacaRestServer->put("/api/v1/switch/0/setswitchname", &setSwitchName);
	m_AlpacaRestServer->put("/api/v1/switch/0/setswitchvalue", &setSwitchValue);

	m_AlpacaRestServer->get("/api/v1/switch/0/statechangecomplete", &switchStateChangeComplete);
	m_AlpacaRestServer->get("/api/v1/switch/0/switchstep", &switchStep);



	//
	// adding our own endpoints for the settings and controls
	//

	m_AlpacaRestServer->use("/setup/useDHCP", &useDHCPState);
	m_AlpacaRestServer->use("/setup/ipAddress", &ipAddressValue);
	m_AlpacaRestServer->use("/setup/subnetMask", &subnetMaskValue);
	m_AlpacaRestServer->use("/setup/ipGateway", &ipGatewayValue);

	// m_AlpacaRestServer->use("/setup/wifiApSSID", &podHotSpotSSID);
	// m_AlpacaRestServer->use("/setup/wifiApPassword", &podHotSpotPAssword);
	// m_AlpacaRestServer->use("/setup/wifiApChannel", &podHotSpotChannel);

	m_AlpacaRestServer->put("/setup/podCalibrate", &podCalibrate);
	m_AlpacaRestServer->get("/setup/environmentData", &environmentData);

	// shutter control
	m_AlpacaRestServer->put("/setup/podOpen", &podOpen);
	m_AlpacaRestServer->put("/setup/podClose", &podClose);
	m_AlpacaRestServer->get("/setup/podState", &podState);

	// Power ports control
	m_AlpacaRestServer->use("/setup/podDC1", &podDC1);
	m_AlpacaRestServer->use("/setup/podDC2", &podDC2);
	m_AlpacaRestServer->use("/setup/podPWM1", &podPWM1);
	m_AlpacaRestServer->use("/setup/podPWM2", &podPWM2);
	m_AlpacaRestServer->use("/setup/podUsbC", &podUsbC);
	// Power usage
	m_AlpacaRestServer->get("/setup/podMainPower", &podMainPower);
	m_AlpacaRestServer->get("/setup/podDC1Power", &podDC1Power);
	m_AlpacaRestServer->get("/setup/podDC2Power", &podDC2Power);
	m_AlpacaRestServer->get("/setup/podPWM1Power", &podPWM1Power);
	m_AlpacaRestServer->get("/setup/podPWM2Power", &podPWM2Power);

	m_AlpacaRestServer->get("/setup/podBatPower", &podBatPower);
	m_AlpacaRestServer->get("/setup/podUsbCPower", &podUsbCPower);
	m_AlpacaRestServer->get("/setup/podMot1Power", &podMot1Power);
	m_AlpacaRestServer->get("/setup/podMot2Power", &podMot2Power);

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
