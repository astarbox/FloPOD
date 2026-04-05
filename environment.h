//
// FLO POD controller
// Environment sensor management
// Copyright © 2026 AStarBox. All rights reserved.
//

#ifndef __ENV_SENSOR__
#define __ENV_SENSOR__
#include <Wire.h>
#include <HDC1080.h>
#include <Protocentral_FDC1004.h>

using HDC1080 = GuL::HDC1080;

class HumTempSensor
{
public:
    HumTempSensor();
    ~HumTempSensor();
    void getTempAndHum(float &temperature, float &humidity);
private:
    HDC1080 *m_hdc;
};

HumTempSensor::HumTempSensor()
{
    m_hdc = new HDC1080(Wire);
    m_hdc->getManufacturerID(); // we can use this to check if device is present.
    m_hdc->resetConfiguration();
    m_hdc->enableHeater();
    m_hdc->setHumidityResolution(GuL::HDC1080::HumidityMeasurementResolution::HUM_RES_14BIT);
    m_hdc->setTemperaturResolution(GuL::HDC1080::TemperatureMeasurementResolution::TEMP_RES_14BIT);
    m_hdc->setAcquisitionMode(GuL::HDC1080::AcquisitionModes::BOTH_CHANNEL);
}

HumTempSensor::~HumTempSensor()
{
}

void HumTempSensor::getTempAndHum(float &temperature, float &humidity)
{
    m_hdc->startAcquisition(GuL::HDC1080::Channel::BOTH);
    vTaskDelay((m_hdc->getConversionTime(GuL::HDC1080::Channel::BOTH)/1000)+1);
    temperature = m_hdc->getTemperature();
    humidity = m_hdc->getHumidity();
}


HumTempSensor *humTempSensor;


class RainSensor
{
public:
    RainSensor();
    ~RainSensor();
    bool isPresent();

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