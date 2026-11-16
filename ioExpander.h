//
// Pulsar Imaging Pod
// IO Expander code for the 2 side boards
// Copyright © 2026 AStarBox. All rights reserved.
//

#ifndef __IO_EXP__
#define __IO_EXP__
#include <PID_v1.h>
#include <TCA6408A.h>
#include "config.h"

#define IO_EXP_ADDR1    0x20
#define IO_EXP_ADDR2    0x21

// both side are wired the same way
// intput
#define MAGSW       0   // bit 0 for PO0
// output
#define LED1        1   // bit 1 for PO1
#define LED2        2   // bit 2 for PO2
#define HEATER_ON   3   // bit 3 for PO3

class ioExpander
{
public:
    ioExpander();
    ~ioExpander();

    bool isIoExpander1Present();
    bool isIoExpander2Present();
    bool checkMagSwitch1();
    bool checkMagSwitch2();
    bool checkMagSwitches();

    void setLed1_Side1(bool bOn);
    void setLed2_Side1(bool bOn);
    void setLedSide1(bool bOn);

    void setLed1_Side2(bool bOn);
    void setLed2_Side2(bool bOn);
    void setLedSide2(bool bOn);

    void setHeater1(bool bOn);
    void setHeater2(bool bOn);
    void setHeaters(bool bOn);

private:
    TCA6408A *ioExp1 = nullptr;
    TCA6408A *ioExp2 = nullptr;

    bool m_bIoExp1Present = false;
    bool m_bIoExp2Present = false;

    SemaphoreHandle_t m_mutex;
};


ioExpander *IoExpanders = nullptr;

ioExpander::ioExpander()
{
    m_mutex = xSemaphoreCreateMutex();

    ioExp1 = new TCA6408A(IO_EXP_ADDR1);
    if (ioExp1->begin() == false) {
        m_bIoExp1Present = false;
    } else {
        // bit 0 is input, other bit are output
        //  Set P0 as inputs, P1–P7 as outputs
        //  0x01 00000001
        ioExp1->setPinMode8(0x01);
        m_bIoExp1Present = true;
        // LED off by default
        setLedSide1(false);

    }
    ioExp2 = new TCA6408A(IO_EXP_ADDR2);
    if (ioExp2->begin() == false) {
        m_bIoExp2Present = false;
    } else {
        // bit 0 is input, other bit are output
        //  Set P0 as inputs, P1–P7 as outputs
        //  0x01 00000001
        ioExp2->setPinMode8(0x01);
        m_bIoExp2Present = true;
        // LED off by default
        setLedSide2(false);
    }
}

ioExpander::~ioExpander()
{

}

bool ioExpander::isIoExpander1Present()
{
    return m_bIoExp1Present;
}

bool ioExpander::isIoExpander2Present()
{
    return m_bIoExp2Present;
}

bool ioExpander::checkMagSwitch1()
{
    uint8_t pinState = 1; // not triggered, in case we can't lock the mutex
	if(xSemaphoreTake(m_mutex, portMAX_DELAY) == pdTRUE) {
        // read MAGSW
        pinState = ioExp1->digitalRead1(MAGSW);
        xSemaphoreGive(m_mutex);
    }
    // check value
    return pinState==1; // true if mag switch is triggered
}

bool ioExpander::checkMagSwitch2()
{
    uint8_t pinState = 1; // not triggered, in case we can't lock the mutex
	if(xSemaphoreTake(m_mutex, portMAX_DELAY) == pdTRUE) {
        // read MAGSW
        pinState = ioExp2->digitalRead1(MAGSW);
        xSemaphoreGive(m_mutex);
    }
    // check value
    return pinState==1; // true if mag switch is triggered
}

bool ioExpander::checkMagSwitches()
{
    bool side1, side2;
    side1 = checkMagSwitch1();
    side2 = checkMagSwitch2();

    return (side1|side2);
}

void ioExpander::setLed1_Side1(bool bOn)
{
    if(!ioExp1)
        return;
    // need to deal with the timeout and retry
	if(xSemaphoreTake(m_mutex, portMAX_DELAY) == pdTRUE) {
        ioExp1->digitalWrite1(LED1, bOn?1:0);
        xSemaphoreGive(m_mutex);
    }
}

void ioExpander::setLed2_Side1(bool bOn)
{
    if(!ioExp1)
        return;
    // need to deal with the timeout and retry
	if(xSemaphoreTake(m_mutex, portMAX_DELAY) == pdTRUE) {
        ioExp1->digitalWrite1(LED2, bOn?1:0);
        xSemaphoreGive(m_mutex);
    }
}

void ioExpander::setLedSide1(bool bOn)
{
    setLed1_Side1(bOn);
    setLed2_Side1(bOn);
}

void ioExpander::setLed1_Side2(bool bOn)
{
     if(!ioExp2)
        return;
   // need to deal with the timeout and retry
	if(xSemaphoreTake(m_mutex, portMAX_DELAY) == pdTRUE) {
        ioExp2->digitalWrite1(LED1, bOn?1:0);
        xSemaphoreGive(m_mutex);
    }
}

void ioExpander::setLed2_Side2(bool bOn)
{
     if(!ioExp2)
        return;
    // need to deal with the timeout and retry
	if(xSemaphoreTake(m_mutex, portMAX_DELAY) == pdTRUE) {
        ioExp2->digitalWrite1(LED2, bOn?1:0);
        xSemaphoreGive(m_mutex);
    }
}

void ioExpander::setLedSide2(bool bOn)
{
    setLed1_Side2(bOn);
    setLed2_Side2(bOn);
}

void ioExpander::setHeater1(bool bOn)
{
    if(!ioExp1)
        return;
    // need to deal with the timeout and retry
	if(xSemaphoreTake(m_mutex, portMAX_DELAY) == pdTRUE) {
    ioExp1->digitalWrite1(HEATER_ON, bOn?1:0);
        xSemaphoreGive(m_mutex);
    }
}

void ioExpander::setHeater2(bool bOn)
{
    if(!ioExp2)
        return;
    // need to deal with the timeout and retry
	if(xSemaphoreTake(m_mutex, portMAX_DELAY) == pdTRUE) {
        ioExp2->digitalWrite1(HEATER_ON, bOn?1:0);
        xSemaphoreGive(m_mutex);
    }
}

void ioExpander::setHeaters(bool bOn)
{
    setHeater1(bOn);
    setHeater2(bOn);
}


#endif