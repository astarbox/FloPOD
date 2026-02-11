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
INA260_MAIN INA(0x46);
// U2  A1 = GND , A0 = SCL => 1000011 => 0x43 // DC1
INA260_DC_1 INA(0x4C);
// U3  A1 = 3v3 , A0 = GND => 1000100 => 0x44 // DC2
INA260_DC_2 INA(0x41);
// U4  A1 = 3v3 , A0 = 3v3 => 1000101 => 0x45// PWM1
INA260_PWM1 INA(0x45);
// U5  A1 = SDA , A0 = SDA => 1001010 => 0x4a // PWM2
INA260_PWM2 INA(0x4A);
// U8  A1 = SDA , A0 = SCL => 1001011 => 0x4b // USB-C
INA260_USB_C INA(0x4E);
// U11 A1 = 3v3 , A0 = SCL => 1000111 => 0x47 // VBat
INA260_BAT INA(0x4D);
// U18 A1 = SCL , A0 = SCL => 1001111 => 0x4f // VMOT 
INA260_VMOT INA(0x4F);
// Extra INA260 : A1 = SCL , A0 = SDA => 1001110 => 0x4e // VMOT2
INA260_VMOT2 INA(0x4E);

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