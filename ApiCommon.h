// Common API functions
//
//  Created by Rodolphe Pineau on 2026/10/19
//  Copyright © 2026 Rodolphe Pineau. All rights reserved.
//
#ifndef __API_COMMON__
#define __API_COMMON__

#define API_VAR_BUF_LEN 256
enum RestShutterStates {A_OPEN=0, A_CLOSED, A_OPENING, A_CLOSING, A_ERROR};

void RestError_x400(JsonDocument RestReso, Response &res)
{
	String sResp;
	RestReso["ErrorNumber"] = 0x400;
	RestReso["ErrorMessage"] = "Not Implemented";
	RestReso["Value"] = false;
	serializeJson(RestReso, sResp);
	res.set("Content-Type", "application/json");
	res.write((uint8_t*)(sResp.c_str()),sResp.length());
}

void RestError_x401(JsonDocument &RestReso, Response &res, String errMsg="Invalid parameters")
{
	String sResp;
	RestReso["ErrorNumber"] = 0x401;
	RestReso["ErrorMessage"] = errMsg;
	serializeJson(RestReso, sResp);
	res.set("Content-Type", "application/json");
	res.write((uint8_t*)(sResp.c_str()),sResp.length());
}

void formDataToJson(Request &req, JsonDocument &FormData)
{
	char name[API_VAR_BUF_LEN];
	char value[API_VAR_BUF_LEN];
	String sName;
	String sValue;
	memset(name,0,API_VAR_BUF_LEN);
	memset(value,0,API_VAR_BUF_LEN);
	while(req.form(name, API_VAR_BUF_LEN-1, value, API_VAR_BUF_LEN-1)){
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


void doSetup(Request &req, Response &res)
{
	JsonDocument AlpacaResp;
	JsonDocument FormData;
	bool bParamsOk = false;
	String sResp;
	String sHTML;
	res.set("Content-Type", "text/html");
	DBPrintln("[ **********" + String(__func__) + "********** ]");
	sHTML = "<!DOCTYPE html>\n<html>\n";
	sHTML += "<head>";
	sHTML += "<title>Pulsar Imaging POD Setup</title>\n";
	sHTML += "</head>\n";
	sHTML += "<body>\n";
	sHTML += "<H1>Pulsar Imaging POD Setup</H1>\n";

	sHTML += "</body>\n</html>\n";
	res.print(sHTML);
}

#endif
