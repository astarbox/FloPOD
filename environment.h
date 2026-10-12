//
// Pulsar Imaging Pod
// Environment sensor management
// Copyright © 2026 AStarBox. All rights reserved.
//

#ifndef __ENV_SENSOR__
#define __ENV_SENSOR__
#include <Wire.h>

#include "config.h"

#include <FDC2x1x.h>
#include <SHT31.h>


#define IT_S_RAINING 65 // value in pF .. for now.
#define LC_INDUCTANCE 	18 // 18uh
#define LC_CAPACITENCE	33 // 33pF

class HumTempSensor
{
public:
	HumTempSensor();
	~HumTempSensor();
	bool isPresent();
	void getTempAndHum(float &temperature, float &humidity);

private:
	bool m_bPresent = false;
	SHT31 *m_Sensor;
};

HumTempSensor *humTempSensor;


HumTempSensor::HumTempSensor()
{
	m_Sensor = new SHT31();
	if(!m_Sensor->begin()) {
		delete m_Sensor;
		m_Sensor = nullptr;
		m_bPresent = false;
		return;
	}
}

HumTempSensor::~HumTempSensor()
{

}

void HumTempSensor::getTempAndHum(float &temperature, float &humidity)
{
	if(m_bPresent)
		m_Sensor->read();
	temperature = m_Sensor->getTemperature();
	humidity = m_Sensor->getHumidity();
}

bool HumTempSensor::isPresent()
{
	return m_bPresent;
}



class RainSensor
{
public:
	RainSensor();
	~RainSensor();
	bool isPresent();
	void enableHeater(bool bOn=true);
	float getValue();
	bool isRaining();
private:
	float convertToPF(long nValue);
	FDC2x1x *capacitanceSensorOne;
	FDC2x1x *capacitanceSensorTwo;
	FDC2x1x_DEVICE deviceOne;
	FDC2x1x_DEVICE deviceTwo;
	bool m_bDevOnePresent = false;
	bool m_bDevTwoPresent = false;
	float m_fLastValue = 0.0f;

};

RainSensor *podRainSensor;

RainSensor::RainSensor()
{
	capacitanceSensorOne = new FDC2x1x(FDC2x1x_I2C_ADDR_0, Wire);
	capacitanceSensorTwo = new FDC2x1x(FDC2x1x_I2C_ADDR_1, Wire);

	//setup first two channels, don't stay in sleep mode, deglitch at 10MHz, internal oscillator, gain=1
	deviceOne = capacitanceSensorOne->begin(0x3, false, FDC2x1x_DEGLITCH_10Mhz, true, FDC2x1x_GAIN_1);
	deviceTwo = capacitanceSensorTwo->begin(0x3, false, FDC2x1x_DEGLITCH_10Mhz, true, FDC2x1x_GAIN_1);

	if (deviceOne != FDC2x1x_DEVICE_INVALID)
		m_bDevOnePresent = false;
	if (deviceTwo != FDC2x1x_DEVICE_INVALID)
		m_bDevTwoPresent = false;
}

RainSensor::~RainSensor()
{

}

bool RainSensor::isPresent()
{
	return (m_bDevOnePresent|m_bDevTwoPresent);
}

void RainSensor::enableHeater(bool bOn)
{
	digitalWrite(HEATER, bOn?1:0);
}

float RainSensor::getValue()
{
	unsigned long nValueOne;
	unsigned long nValueTwo;
	nValueOne = capacitanceSensorOne->getReading(0);
	nValueTwo = capacitanceSensorTwo->getReading(0);
	// convert to pF

	// return the highest value as only one of the 2 sensor might have detected rain
	m_fLastValue = nValueOne>nValueTwo?float(nValueOne):float(nValueTwo);
	return m_fLastValue;
}

float convertToPF(long nValue)
{
	float pi = 3.14159265358979323846f;
	float lHenries = LC_INDUCTANCE * 1.0e-6f;
	float omega = 2.0f * pi * 10000000; // Frq is 10Mhz
	float totalFarads = 1.0f / (omega * omega * lHenries);
	float totalPf = totalFarads * 1.0e12f;
	float derivedPf = totalPf - LC_CAPACITENCE;

  if ( derivedPf < 0.0f) {
    // Invalid LC parameters produced negative derived capacitance
    return -1.0;
  }
  return derivedPf;
}

bool RainSensor::isRaining()
{
	bool bIsRaining = false;
	float fRainValue;
	fRainValue = getValue();

	if( fRainValue > IT_S_RAINING)
		bIsRaining = true;

	return bIsRaining;
}

#endif // __ENV_SENSOR__