//
// Pulsar Imaging Pod
// Environment sensor management
// Copyright © 2026 AStarBox. All rights reserved.
//

#ifndef __ENV_SENSOR__
#define __ENV_SENSOR__
#include <Wire.h>
#include <Protocentral_FDC1004.h>
#include "config.h"
#ifdef USE_HDC1080
#include <HDC1080.h>
using HDC1080 = GuL::HDC1080;
#else
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

#ifdef USE_HDC1080
	HDC1080 *m_Sensor;
#else
	SHT31 *m_Sensor;
#endif
};

HumTempSensor::HumTempSensor()
{
#ifdef USE_HDC1080
	m_Sensor = new HDC1080(Wire);
	m_Sensor->getManufacturerID(); // we can use this to check if device is present.
	m_Sensor->resetConfiguration();
	m_Sensor->enableHeater();
	m_Sensor->setHumidityResolution(GuL::HDC1080::HumidityMeasurementResolution::HUM_RES_14BIT);
	m_Sensor->setTemperaturResolution(GuL::HDC1080::TemperatureMeasurementResolution::TEMP_RES_14BIT);
	m_Sensor->setAcquisitionMode(GuL::HDC1080::AcquisitionModes::BOTH_CHANNEL);
#else
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
#ifdef USE_HDC1080
	m_Sensor->startAcquisition(GuL::HDC1080::Channel::BOTH);
	vTaskDelay((m_Sensor->getConversionTime(GuL::HDC1080::Channel::BOTH)/1000)+1);
#else
	m_Sensor->read();
#endif
	temperature = m_Sensor->getTemperature();
	humidity = m_Sensor->getHumidity();
}


HumTempSensor *humTempSensor;


class RainSensor
{
public:
	RainSensor();
	~RainSensor();
	bool isPresent();
	void enableHeater(bool bOn=true);
	float getValue();
private:
	FDC1004 *capacitanceSensor;
	float m_fLastValue = 0.0f;
	bool m_bPresent = false;

};

RainSensor::RainSensor()
{
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
}

RainSensor::~RainSensor()
{

}

bool RainSensor::isPresent()
{
	return m_bPresent;
}

void RainSensor::enableHeater(bool bOn)
{
	digitalWrite(HEATER, bOn?1:0);
}

float RainSensor::getValue()
{
	if(m_bPresent) {
		fdc1004_capacitance_t measurement = capacitanceSensor->getCapacitanceMeasurement(FDC1004_CHANNEL_0);
		// Check if FDC1004 has returned a proper value
		if (!isnan(measurement.capacitance_pf)) {
			m_fLastValue = measurement.capacitance_pf;
		}
	}
	return m_fLastValue;
}

RainSensor *podRainSensor;

#endif // __ENV_SENSOR__