//
// FLO POD controller power management
//
// Copyright © 2026 AStarBox. All rights reserved.
//
#ifndef __POWER_PORT_
#define __POWER_PORT_
#include "INA260.h"

// define all INA260 addresses
/*
MCP3421 -> 1101000 -> 0x68
HDC1080 -> 1000000 -> 0x40 , this will conflict with U1, U1 address need to be changed,
					we can't have 2 I2C devices on the same address

AS5048B -> 1000001 -> 0x41
*/
// U1  A1 = 3v3 , A0 = SDA => 1000110 => 0x46// Main
INA260 INS260_MAIN(0x46);
// U2  A1 = GND , A0 = SCL => 1000011 => 0x43 // DC1
INA260 INS260_DC_1(0x4C);
// U3  A1 = 3v3 , A0 = GND => 1000100 => 0x44 // DC2
INA260 INS260_DC_2(0x41);
// U4  A1 = 3v3 , A0 = 3v3 => 1000101 => 0x45// PWM1
INA260 INS260_PWM1(0x45);
// U5  A1 = SDA , A0 = SDA => 1001010 => 0x4a // PWM2
INA260 INS260_PWM2(0x4A);
// U8  A1 = SDA , A0 = SCL => 1001011 => 0x4b // USB-C
INA260 INS260_USB_C(0x4E);
// U11 A1 = 3v3 , A0 = SCL => 1000111 => 0x47 // VBat
INA260 INS260_BAT(0x4D);
// U18 A1 = SCL , A0 = SCL => 1001111 => 0x4f // VMOT
INA260 INS260_VMOT(0x4F);
// Extra INA260 : A1 = SCL , A0 = SDA => 1001110 => 0x4e // VMOT2
INA260 INS260_VMOT2(0x4E);

class powerPort
{
public:
	powerPort();
	bool setAlarmAmps(INA260 *INA, int nAmps);
	bool setAlarmVoltage(INA260 *INA, int nVolts);

private:
};

powerPort *podPowerController = nullptr;

powerPort::powerPort()
{
	if (!INS260_MAIN.begin()) {
		// set error.. this one is not responding
	}

	if (!INS260_DC_1.begin()) {
		// set error.. this one is not responding
	}

	if (!INS260_DC_2.begin()) {
		// set error.. this one is not responding
	}
	if (!INS260_USB_C.begin()) {
		// set error.. this one is not responding
	}

	if (!INA260_PWM1.begin()) {
		// set error.. this one is not responding
	}
	if (!INA260_PWM2.begin()) {
		// set error.. this one is not responding
	}
	if (!INA260_BAT.begin()) {
		// set error.. this one is not responding
	}
}

bool powerPort::setAlarmAmps(INA260 *INA, int nAmps)
{
	uint16_t alert_mask = INA260_SHUNT_OVER_CURRENT;
	INA->setAlertLimit(nAmps);
	uint16_t test_limit = INA->getAlertLimit();
	if (test_limit != limit) {
		return false;
	}

	INA->setAlertRegister(alert_mask);
	return true;
}

bool powerPort::setAlarmVoltage(INA260 *INA, int nVolts)
{
	uint16_t alert_mask = INA260_BUS_OVER_VOLTAGE;
	INA->setAlertLimit(nAmps);
	uint16_t test_limit = INA->getAlertLimit();
	if (test_limit != limit) {
		return false;
	}
	INA->setAlertRegister(nVolts);
	return true;
}

#endif