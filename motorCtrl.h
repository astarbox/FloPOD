//
// FLO POD controller
// Motor and encoder control
// Copyright © 2026 AStarBox. All rights reserved.
//

#ifndef __MOT_ENC__
#define __MOT_ENC__
// #include <JMotor.h>


#include "config.h"

#define AS5048B_ADDR	0x41

enum MotorStates {M_STOPPED, M_RUNNING, M_CALIBRATING};

class motorCtrl
{
public:
	motorCtrl();
	void 	Calibrate();
	void 	Open();
	void 	Close();
	void 	getState(podStates &nState);
private:
	EncoderConfig m_EncoderConfig;
	podStates m_nState = IDLE;
	MotorStates m_nMotorState = M_STOPPED;
	void	getEncoderPosition(float &fDegrees);


};

motorCtrl *PodMotorController = nullptr;

motorCtrl::motorCtrl()
{
	// init dir pin and led pwm pin
	// attach interrupt for motor over current
	// get power up state;
	// read encoder
	// compare with open/close position
	// if in middle, set error, this will trigger a close
	m_nState = IDLE;
	m_nMotorState = M_STOPPED;
}

void motorCtrl::Calibrate()
{
	// set speed to 10%
	// close

}

void motorCtrl::Open()
{
	// move to calibrated open position
	m_nState = OPENING;
}

void motorCtrl::Close()
{
	// move to calibrated close position
	m_nState = CLOSING;
}

void motorCtrl::getState(podStates &nState)
{
	// get current position in degree as well as podStates;
	nState = m_nState;

	// if the stated is error, try to close
	if(m_nState == POD_ERROR) {

	}

}

void motorCtrl::getEncoderPosition(float &fDegrees)
{

}

#endif