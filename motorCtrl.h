//
// FLO POD controller
// Motor and encoder control
// Copyright © 2026 AStarBox. All rights reserved.
//

#ifndef __MOT_ENC__
#define __MOT_ENC__
#include <PID_v1.h>

#include "config.h"
#include "ams_as5048b.h"

#define AS5048B_ADDR	0x41

enum MotorStates {M_STOPPED, M_RUNNING, M_CALIBRATING};
enum MotorCalibrationSteps {CAL_NONE, CAL_FIRST_CLOSE,CAL_OPENING, CAL_FINISH_CLOSE};
class motorCtrl
{
public:
	motorCtrl();
	void 	Calibrate();
	void 	Open();
	void 	Close();
	void	Stop();
	void	Run();
	void 	getState(MotorCalibrationSteps &nCalState);
	void	OverCurrentStop();
	bool	bIsEncoderCalibrated();
	void	getEncoderPosition(float &fDegrees);

private:
	void	motorMove(float fPosition);

	EncoderConfig m_EncoderConfig;
	MotorStates m_nMotorState = M_STOPPED;
	MotorCalibrationSteps m_CalsState = CAL_NONE;

	AMS_AS5048B *m_AMS_AS5048B;

	double	m_fEncoderValue;
	double	m_fTargetPosition;

	double Kp = 2.00;	// Proportional gain — how strongly the controller reacts to the current error
	double Ki = 5.00;	// Integral gain — how strongly it reacts to accumulated error over time
	double Kd = 1.00;	// Derivative gain — how strongly it reacts to the rate of error change

	double 	pid_input = 0;
	double	pid_output = 0;
	PID 	*myPID = nullptr;
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
	m_nMotorState = M_STOPPED;
	if(globalPodConfig) {
		globalPodConfig->LoadEncoderConfig(m_EncoderConfig);
	}
	m_AMS_AS5048B = new AMS_AS5048B();
	m_AMS_AS5048B->begin();
	myPID = new PID(&pid_input, &pid_output, &m_fTargetPosition, Kp, Ki, Kd, DIRECT);
	myPID->SetMode(AUTOMATIC);    // Enable PID
	myPID->SetOutputLimits(0, 100); // Limit PWM output range to 100%
}

void motorCtrl::Calibrate()
{
	switch(m_CalsState) {
		case CAL_NONE:
			// set speed to 10%
			m_nMotorState = M_CALIBRATING;
			// close
			m_CalsState = CAL_FIRST_CLOSE;
			Close();
			break;

		case CAL_FIRST_CLOSE:
			// store closed value of encoder
			// open
			m_CalsState = CAL_OPENING;
			Open();
			break;

		case CAL_OPENING:
			// store open value of encoder
			// set speed to normal speed
			m_CalsState = CAL_FINISH_CLOSE;
			// close
			Close();
			break;

		case CAL_FINISH_CLOSE:
			Stop();
			m_nMotorState = M_STOPPED;
			m_CalsState = CAL_NONE;
			m_EncoderConfig.bIsCalibrated = true;
			break;
	}
}


void motorCtrl::Open()
{
	// move to calibrated open position
	motorMove(m_EncoderConfig.openAngle);
}

void motorCtrl::Close()
{
	// move to calibrated close position
	motorMove(m_EncoderConfig.closeAngle);
}

void motorCtrl::Stop()
{

}

void motorCtrl::Run()
{
	pid_input = m_AMS_AS5048B->angleR(U_DEG);
	myPID->Compute();
}

void motorCtrl::getState(MotorCalibrationSteps &nCalState)
{
	// get current position in degree as well as podStates;
	nCalState = m_CalsState;
}


void motorCtrl::getEncoderPosition(float &fDegrees)
{
	fDegrees = float(m_AMS_AS5048B->angleR(U_DEG, true));
}

bool motorCtrl::bIsEncoderCalibrated()
{
	return m_EncoderConfig.bIsCalibrated;
}


void motorCtrl::motorMove(float fPosition)
{
	m_fTargetPosition = fPosition;
}


#endif