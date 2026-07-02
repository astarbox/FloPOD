//
// Pulsar Imaging Pod
// Environment sensor management
// Copyright © 2026 AStarBox. All rights reserved.
//

#ifndef __ENV_SENSOR__
#define __ENV_SENSOR__
#include <Wire.h>

#include "config.h"

#if defined(USE_FDC1004)
#include <Protocentral_FDC1004.h>
#elif defined(USE_FDC2112)
#include <FDC2x1x.h>
#endif

#if defined(USE_HDC1080)
#include <HDC1080.h>
using HDC1080 = GuL::HDC1080;
#elif defined(USE_SHT30)
#include <SHT31.h>
#endif 


#define IT_S_RAINING 65 // value in pF .. for now.


class HumTempSensor
{
public:
	HumTempSensor();
	~HumTempSensor();
	bool isPresent();
	void getTempAndHum(float &temperature, float &humidity);	

private:
		bool m_bPresent = false;

#if defined(USE_HDC1080)
	HDC1080 *m_Sensor;
#elif defined(USE_SHT30)
	SHT31 *m_Sensor;
#endif
};

HumTempSensor *humTempSensor;


HumTempSensor::HumTempSensor()
{
#if defined(USE_HDC1080)
	m_Sensor = new HDC1080(Wire);
	// we can use this to check if device is present.
	if(!m_Sensor->getManufacturerID()){
		delete m_Sensor;
		m_Sensor = nullptr;
		m_bPresent = false;
		return;
	} 
	else {
		m_bPresent = true;
		m_Sensor->resetConfiguration();
		m_Sensor->enableHeater();
		m_Sensor->setHumidityResolution(GuL::HDC1080::HumidityMeasurementResolution::HUM_RES_14BIT);
		m_Sensor->setTemperaturResolution(GuL::HDC1080::TemperatureMeasurementResolution::TEMP_RES_14BIT);
		m_Sensor->setAcquisitionMode(GuL::HDC1080::AcquisitionModes::BOTH_CHANNEL);
	}
#elif defined(USE_SHT30)
	m_Sensor = new SHT31();
	if(!m_Sensor->begin()) {
		delete m_Sensor;
		m_Sensor = nullptr;
		m_bPresent = false;
		return;
	}
#endif
}

HumTempSensor::~HumTempSensor()
{
}

void HumTempSensor::getTempAndHum(float &temperature, float &humidity)
{
#if defined(USE_HDC1080)
	m_Sensor->startAcquisition(GuL::HDC1080::Channel::BOTH);
	vTaskDelay((m_Sensor->getConversionTime(GuL::HDC1080::Channel::BOTH)/1000)+1);
#elif defined(USE_SHT30)
	if(m_bPresent)
		m_Sensor->read();
#endif
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
#if defined(USE_FDC1004)
	FDC1004 *capacitanceSensor;
	bool m_bPresent = false;
#elif defined(USE_FDC2112)
	FDC2x1x *capacitanceSensorOne;
	FDC2x1x *capacitanceSensorTwo;
	FDC2x1x_DEVICE deviceOne;
	FDC2x1x_DEVICE deviceTwo;
	bool m_bDevOnePresent = false;
	bool m_bDevTwoPresent = false;
#endif

	float m_fLastValue = 0.0f;

};

RainSensor *podRainSensor;

RainSensor::RainSensor()
{
#if defined(USE_FDC1004)
	capacitanceSensor = new FDC1004(FDC1004_RATE_100HZ);
	if(!capacitanceSensor->begin()) {
		m_bPresent = false;
		return;
	}
	if (!capacitanceSensor->isConnected()) {
		m_bPresent = false;
		return;
	}
		m_bPresent = true;
#elif defined(USE_FDC2112)
	capacitanceSensorOne = new FDC2x1x(FDC2x1x_I2C_ADDR_0, Wire);
	capacitanceSensorTwo = new FDC2x1x(FDC2x1x_I2C_ADDR_1, Wire);

	//setup first two channels, don't stay in sleep mode, deglitch at 10MHz, internal oscillator, gain=1
	deviceOne = capacitanceSensorOne->begin(0x3, false, FDC2x1x_DEGLITCH_10Mhz, true, FDC2x1x_GAIN_1);
	deviceTwo = capacitanceSensorTwo->begin(0x3, false, FDC2x1x_DEGLITCH_10Mhz, true, FDC2x1x_GAIN_1);

	if (deviceOne != FDC2x1x_DEVICE_INVALID) 
		m_bDevOnePresent = false;
	if (deviceTwo != FDC2x1x_DEVICE_INVALID) 
		m_bDevTwoPresent = false;
#endif
}

RainSensor::~RainSensor()
{

}

bool RainSensor::isPresent()
{
#if defined(USE_FDC1004)
	return m_bPresent;
#elif defined(USE_FDC2112)
	return (m_bDevOnePresent|m_bDevTwoPresent);
#endif
}

void RainSensor::enableHeater(bool bOn)
{
	digitalWrite(HEATER, bOn?1:0);
}

float RainSensor::getValue()
{
#if defined(USE_FDC1004)
	if(m_bPresent) {
		fdc1004_capacitance_t measurement = capacitanceSensor->getCapacitanceMeasurement(FDC1004_CHANNEL_0);
		// Check if FDC1004 has returned a proper value
		if (!isnan(measurement.capacitance_pf)) {
			m_fLastValue = measurement.capacitance_pf;
		}
	}
#elif defined(USE_FDC2112)
	unsigned long nValueOne;
	unsigned long nValueTwo;
	nValueOne = capacitanceSensorOne->getReading(0);
	nValueTwo = capacitanceSensorTwo->getReading(0);
	// return the highest value as only one of the 2 sensor might have detected rain
	m_fLastValue = nValueOne>nValueTwo?float(nValueOne):float(nValueTwo);
#endif
	return m_fLastValue;
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