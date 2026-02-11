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
// U1  INA260 A0=GND A1=GND -> 1000000 -> 0x40 // Main
INA260_MAIN INA(0x40);
// U2  INA260 A0=SCL A1=GND -> 1001100 -> 0x4c // DC1
INA260_DC_1 INA(0x4C);
// U3  INA260 A0=GND A1=3V3 -> 1000001 -> 0x41 // DC2
INA260_DC_2 INA(0x41);
// U4  INA260 A0=3V3 A1=3V3 -> 1000101 -> 0x45 // PWM1
INA260_PWM1 INA(0x45);
// U5  INA260 A0=SDA A1=SDA -> 1001010 -> 0x4a // PWM2
INA260_PWM2 INA(0x4A);
// U8  INA260 A0=SCL A1=SDA -> 1001110 -> 0x4e // USB-C
INA260_USB_C INA(0x4E);
// U11 INA260 A0=SCL A1=3V3 -> 1001101 -> 0x4d // VBat
INA260_BAT INA(0x4D);

class powerPort {
public:
  powerPort();
  void setAlarmAmps(INA Port, int nAmps);
  void setAlarmVoltage(INA Port, int nVolts);
private:

};

powerPort *podPowerController = nullptr;

powerPort::powerPort()
{
  if (!INA260_MAIN.begin() ) {
    // set error.. this one is not responding
  }

  if (!INA260_DC_1.begin() ) {
    // set error.. this one is not responding
  }

  if (!INA260_DC_2.begin() ) {
    // set error.. this one is not responding
  }
  if (!INA260_USB_C.begin() ) {
    // set error.. this one is not responding
  }

  if (!INA260_PWM1.begin() ) {
    // set error.. this one is not responding
  }
  if (!INA260_PWM2.begin() ) {
    // set error.. this one is not responding
  }
  if (!INA260_BAT.begin() ) {
    // set error.. this one is not responding
  }
}


void powerPort::setAlarmAmps(INA Port, int nAmps)
{

}

void powerPort::setAlarmVoltage(INA Port, int nVolts)
{

}

#endif