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

#define FORWARD	0
#define REVERSE	1

enum MotorStates {M_STOPPED, M_RUNNING, M_CALIBRATING};
enum PodShutterState {PS_UNKNOWN, PS_CLOSED, PS_OPEN, PS_CLOSING, PS_OPENING};
enum MotorCalibrationSteps {CAL_NONE, CAL_FIRST_CLOSE,CAL_OPENING, CAL_FINISH_CLOSE, CAL_DONE};
class motorCtrl
{
public:
	motorCtrl();
	void 	Calibrate();
	void 	Open();
	void 	Close();
	void	Stop();
	void	Run();
	void 	getCalState(MotorCalibrationSteps &nCalState);
	void	getShutterState(PodShutterState &nPsState);
	void	OverCurrentStop();
	bool	bIsEncoderCalibrated();
	void	getEncoderPosition(float &fDegrees);
private:
	void	motorMoveTo(double fPosition);
	bool	checkBoundaries(float dTarget, float dCurentPos, float dMargin);

	EncoderConfig m_EncoderConfig;
	MotorStates m_nMotorState = M_STOPPED;
	MotorCalibrationSteps m_CalsState = CAL_NONE;
	PodShutterState m_nPsState = PS_UNKNOWN;

	AMS_AS5048B *m_AMS_AS5048B;

	double	m_dEncoderValue;
	double	m_dTargetPosition;

	// these will need to be set once I can test on the real hardware.
	double m_dKp = 2.00;	// Proportional gain — how strongly the controller reacts to the current error
	double m_dKi = 5.00;	// Integral gain — how strongly it reacts to accumulated error over time
	double m_dKd = 1.00;	// Derivative gain — how strongly it reacts to the rate of error change

	double	m_dPidOutput = 0;
	PID 	*myPID = nullptr;
};

motorCtrl *PodMotorController = nullptr;

motorCtrl::motorCtrl()
{
	// init dir pin and led pwm pin
	ledcAttachChannel(MOT_EN, PWM_FREQ, LEDC_TIMER_12_BIT, MotorPwmChannel);
	ledcWriteChannel(MotorPwmChannel, 0); // make sure we're not moving.
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
	myPID = new PID(&m_dEncoderValue, &m_dPidOutput, &m_dTargetPosition, m_dKp, m_dKi, m_dKd, DIRECT);
	myPID->SetMode(AUTOMATIC);    // Enable PID
	myPID->SetOutputLimits(-255, 255); // Limit output to -255 to 255 as it's the PWM ratio
}

void motorCtrl::Calibrate()
{

	if(m_CalsState == CAL_NONE && m_nMotorState == M_STOPPED) {
		// set speed to 10%
		myPID->SetOutputLimits(-25, 25);
		m_nMotorState = M_CALIBRATING;
		// close
		m_CalsState = CAL_FIRST_CLOSE;
		Close();
	}
	else if (m_CalsState == CAL_FIRST_CLOSE && m_nMotorState == M_STOPPED) {
		// we just hit the close position.
		// store closed value of encoder
		getEncoderPosition(m_EncoderConfig.closeAngle);
		// open
		m_CalsState = CAL_OPENING;
		Open();
		// else we're still closing.
	}
	else if (m_CalsState == CAL_OPENING && m_nMotorState == M_STOPPED) {
		// we just hit the open position.
		// store closed value of encoder
		getEncoderPosition(m_EncoderConfig.openAngle);
		// open
		m_CalsState = CAL_FINISH_CLOSE;
		Open();
		// else we're still opening.
	}
	else if (m_CalsState == CAL_FINISH_CLOSE && m_nMotorState == M_STOPPED) {
		// we just hit the close position.
		Stop();
		m_CalsState = CAL_DONE;
		m_EncoderConfig.bIsCalibrated = true;
		if(globalPodConfig) {
			globalPodConfig->saveEncoderConfig(m_EncoderConfig);
		}
		myPID->SetOutputLimits(-255, 255);
		// else we're still closinf.
	}
}


void motorCtrl::Open()
{
	// set direction
	// move to calibrated open position
	motorMoveTo(m_EncoderConfig.openAngle);
	m_nMotorState = M_RUNNING;
	m_nPsState = PS_OPENING;

}

void motorCtrl::Close()
{
	// set direction
	// move to calibrated close position
	motorMoveTo(m_EncoderConfig.closeAngle);
	m_nMotorState = M_RUNNING;
	m_nPsState = PS_CLOSING;
}

void motorCtrl::Stop()
{
	ledcWriteChannel(MotorPwmChannel, 0); // make sure we're not moving.
	m_nMotorState = M_STOPPED;
}

void motorCtrl::Run()
{
	double newPWM = 0;

	if(m_nMotorState == M_STOPPED)
		return;

	m_dEncoderValue = m_AMS_AS5048B->angleR(U_DEG);
	myPID->Compute();
	DBPrintln("m_dPidOutput = " + String(m_dPidOutput));

	// are we at the target position ?
	// Yes -> stop motor PWM
	if (checkBoundaries(m_dTargetPosition, m_dEncoderValue, 0.1)) {
		Stop();
		switch(m_nPsState) {
			case PS_OPENING:
				m_nPsState = PS_OPEN;
				break;
			case PS_CLOSING:
				m_nPsState = PS_CLOSED;
				break;
			default:
				m_nPsState = PS_UNKNOWN;
		}
	}
	// No -> set motor ouput PWM
	else {
		newPWM = fabs(m_dPidOutput);
		DBPrintln("newPWM = " + String(newPWM));

		if(m_dPidOutput<0) {
			// set directiobn to reverse
			digitalWrite(MOT_PH, REVERSE);
		}
		else {
			// set directiobn to forward
			digitalWrite(MOT_PH, FORWARD);
		}
		ledcWriteChannel(MotorPwmChannel, newPWM); // make sure we're not moving.
	}
}

void motorCtrl::getCalState(MotorCalibrationSteps &nCalState)
{
	// get current position in degree as well as podStates;
	nCalState = m_CalsState;
}

void motorCtrl::getShutterState(PodShutterState &nPsState)
{
	nPsState = m_nPsState;
}

void motorCtrl::OverCurrentStop()
{
		ledcWriteChannel(MotorPwmChannel, 0); // Motor is stalling, stop everyting
		m_nMotorState = M_STOPPED;
}


void motorCtrl::getEncoderPosition(float &fDegrees)
{
	fDegrees = float(m_AMS_AS5048B->angleR(U_DEG, true));
}

bool motorCtrl::bIsEncoderCalibrated()
{
	return m_EncoderConfig.bIsCalibrated;
}

void motorCtrl::motorMoveTo(double dPosition)
{
	m_dTargetPosition = dPosition;
}

bool motorCtrl::checkBoundaries(float fTarget, float fCurentPos, float fMargin)
{
	int highMark;
	int lowMark;
	int roundedTarget;

	highMark = int((fCurentPos+fMargin) * 100);
	lowMark = int((fCurentPos-fMargin) * 100);
	roundedTarget = int((fTarget) * 100);

	if (roundedTarget > lowMark && roundedTarget <= highMark) {
		return true;
	}

	return false;
}

#endif