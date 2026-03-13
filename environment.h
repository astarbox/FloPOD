//
// FLO POD controller
// Environment sensor management
// Copyright © 2026 AStarBox. All rights reserved.
//

#ifndef __ENV_SENSOR__
#define __ENV_SENSOR__
#include <HDC1080.h>
#include <Adafruit_MCP3421.h>
#include <Wire.h>

using HDC1080 = GuL::HDC1080;

#define MCP3421_ADDR    0x68

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

    int32_t getADCValue();
private:
    Adafruit_MCP3421 m_mcp;
    int32_t m_nLastValue;

};

RainSensor::RainSensor()
{
    m_mcp.begin(MCP3421_ADDR);
    m_mcp.setGain(GAIN_1X);
    m_mcp.setResolution(RESOLUTION_18_BIT); // 3.75 SPS
    m_mcp.setMode(MODE_CONTINUOUS);
}

RainSensor::~RainSensor()
{

}

int32_t RainSensor::getADCValue()
{
    // Check if MCP3421 has completed a conversion in continuous mode
    if (m_mcp.isReady()) {
        m_nLastValue = m_mcp.readADC(); // Read ADC value;
    }
    return m_nLastValue;
}

RainSensor *podRainSensor;

#endif // __ENV_SENSOR__