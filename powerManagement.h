#include <sys/types.h>
#include <sys/_stdint.h>
//
// FLO POD controller power management
//
// Copyright © 2026 AStarBox. All rights reserved.
//
#ifndef __POWER_PORT_
#define __POWER_PORT_
#include "INA260.h"

#include "config.h"

// define all INA260 addresses
/*
MCP3421 -> 1101000 -> 0x68
HDC1080 -> 1000000 -> 0x40 , this will conflict with U1, U1 address need to be changed,
					we can't have 2 I2C devices on the same address

AS5048B -> 1000001 -> 0x41
*/
// U2  A1 = GND , A0 = SCL => 1000011 => 0x43 // DC1
INA260 INA260_DC_1(0x43);

// U3  A1 = 3v3 , A0 = GND => 1000100 => 0x44 // DC2 -> going to SHT30, will move to 0x48
INA260 INA260_DC_2(0x44);

// U4  A1 = 3v3 , A0 = 3v3 => 1000101 => 0x45// PWM1
INA260 INA260_PWM1(0x45);

// U1  A1 = 3v3 , A0 = SDA => 1000110 => 0x46// Main
INA260 INA260_MAIN(0x46);

// U11 A1 = 3v3 , A0 = SCL => 1000111 => 0x47 // VBat
INA260 INA260_BAT(0x47);

// U5  A1 = SDA , A0 = SDA => 1001010 => 0x4a // PWM2
INA260 INA260_PWM2(0x4A);

// U8  A1 = SDA , A0 = SCL => 1001011 => 0x4b // USB-C
INA260 INA260_USB_C(0x4B);

// U18 A1 = SCL , A0 = SCL => 1001111 => 0x4f // VMOT
INA260 INA260_VMOT(0x4F);

// Extra INA260 : A1 = SCL , A0 = SDA => 1001110 => 0x4e // VMOT2
INA260 INA260_VMOT2(0x4E);

class powerPorts
{
public:
	powerPorts();
	bool setAlarmAmps(INA260 &INA, float nAmps);
	bool setAlarmVoltage(INA260 &INA, float nVolts);
	void setPortState(int nPort, bool bOn);
	bool setPortToPWM(int nPort, int nChanne);
	bool setPwmPortState(int nPort, int nPercent);
	bool checkAlert(INA260 &INA);

	float readVolts(INA260 &INA);
	float readAmps(INA260 &INA);
	float readPower(INA260 &INA);

	bool bMainInaPresent = false;
	bool bDC1InaPresent = false;
	bool bDC2InaPresent = false;
	bool bPWM1InaPresent = false;
	bool bPWM2InaPresent = false;
	bool bUSBCInaPresent = false;
	bool bBATInaPresent = false;
	bool bVMOTInaPresent = false;
	bool bVMOT2InaPresent = false;

private:
	float rawToAmps(int16_t value);
	int16_t ampsToRaw(float value);
	float rawToVolts(uint16_t value);
	uint16_t voltsToRaw(float value);
	float rawToWatts(uint16_t value);
	uint16_t wattsToRaw(float value);

	SemaphoreHandle_t m_xSemaphore = NULL;
};

powerPorts *podPowerController = nullptr;

powerPorts::powerPorts()
{

	if (!INA260_MAIN.begin()) {
		// set error.. this one is not responding
	}
	else
		bMainInaPresent = true;

	if (!INA260_DC_1.begin()) {
		// set error.. this one is not responding
	}
	else
		bDC1InaPresent = true;

	if (!INA260_DC_2.begin()) {
		// set error.. this one is not responding
	}
	else
		bDC2InaPresent = true;

	if (!INA260_USB_C.begin()) {
		// set error.. this one is not responding
	}
	else
		bUSBCInaPresent = true;

	if (!INA260_PWM1.begin()) {
		// set error.. this one is not responding
	}
	else
		bPWM1InaPresent = true;

	if (!INA260_PWM2.begin()) {
		// set error.. this one is not responding
	}
	else
		bPWM2InaPresent = true;

	if (!INA260_BAT.begin()) {
		// set error.. this one is not responding
	}
	else
		bBATInaPresent = true;

	if (!INA260_VMOT.begin()) {
		// set error.. this one is not responding
	}
	else
		bVMOTInaPresent = true;

	if (!INA260_VMOT2.begin()) {
		// set error.. this one is not responding
	}
	else
		bVMOT2InaPresent = true;

	// set PWM pins
	setPortToPWM(PWM1,PWM1PwmChannel);
	setPwmPortState(PWM1PwmChannel,0);
	setPortToPWM(PWM2,PWM2PwmChannel);
	setPwmPortState(PWM2PwmChannel,0);
}

bool powerPorts::setAlarmAmps(INA260 &INA, float nAmps)
{
	uint16_t alert_mask = INA260_SHUNT_OVER_CURRENT;
	// set value in mA
	uint16_t limit = ampsToRaw(nAmps);
	INA.setAlertLimit(limit);
	uint16_t test_limit = INA.getAlertLimit();
	if (test_limit != limit) {
		return false;
	}

	INA.setAlertRegister(alert_mask);
	INA.setAlertLatchEnable(true);
	return true;
}

bool powerPorts::setAlarmVoltage(INA260 &INA, float nVolts)
{
	uint16_t alert_mask = INA260_BUS_OVER_VOLTAGE;
	// set value in mV
	uint16_t limit = voltsToRaw(nVolts);
	INA.setAlertLimit(limit);
	uint16_t test_limit = INA.getAlertLimit();
	if (test_limit != limit) {
		return false;
	}
	INA.setAlertRegister(alert_mask);
	INA.setAlertLatchEnable(true);
	return true;
}


void powerPorts::setPortState(int nPort, bool bOn)
{
	digitalWrite(nPort, bOn?1:0);
}

bool powerPorts::setPortToPWM(int nPort, int nChannel)
{
	bool bOk = true;
	bOk = ledcAttachChannel(nPort, PWM_FREQ, LEDC_TIMER_12_BIT, nChannel);
	return bOk;
}

bool powerPorts::setPwmPortState(int nChannel, int nPercent)
{
	bool bOk = true;
	int dutyCycle = int((float(nPercent)/100.0f) * MAX_DUTY_CYCLE);

	if(dutyCycle > MAX_DUTY_CYCLE)
		dutyCycle = MAX_DUTY_CYCLE;
	bOk = ledcWriteChannel(nChannel, dutyCycle);
	return bOk;
}

bool powerPorts::checkAlert(INA260 &INA)
{
	bool bAlertTRiggered = false;
	uint16_t flags = INA.getAlertRegister();
	if(flags) {
		bAlertTRiggered = true;
	}
	return bAlertTRiggered;
}

float powerPorts::readVolts(INA260 &INA)
{
	float fValue;
	fValue = INA.getBusVoltage();
	return fValue;
}

float powerPorts::readAmps(INA260 &INA)
{
	float fValue;
	fValue = INA.getCurrent();
	return fValue;
}

float powerPorts::readPower(INA260 &INA)
{
	float fValue;
	fValue = INA.getPower();
	return fValue;
}

float powerPorts::rawToAmps(int16_t value)
{
	return (value * 1.25) / 1000.0;
}

int16_t powerPorts::ampsToRaw(float value)
{
	return (value * 1000.0) / 1.25;
}

float powerPorts::rawToVolts(uint16_t value)
{
	return (value * 1.25) / 1000.0;
}

uint16_t powerPorts::voltsToRaw(float value)
{
	return (value * 1000.0) / 1.25;
}

float powerPorts::rawToWatts(uint16_t value)
{
	return (value * 10.0) / 1000.0;
}

uint16_t powerPorts::wattsToRaw(float value)
{
	return (value * 1000.0) / 10.0;
}

#endif