//
// FLO POD controller
// Motor and encoder control
// Copyright © 2026 AStarBox. All rights reserved.
//

#ifndef __MOT_ENC__
#define __MOT_ENC__
// #include <JMotor.h>

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

	float	m_fEncoderValue;
	float	m_fTargetPosition;
	float 	m_e;
	float	m_e_prev = 0;
	float	m_inte;
	float	m_inte_prev = 0;
	float	m_fEncoderValue_prev;
	unsigned long	m_t_prev = 0;

	// sign will be used for direction
	float	m_Vmax = 12;
	float	m_Vmin = -12; 
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
	float kp = 0.2;
	float ki = 0.00000 ;
	float kd = 2.00;
	float Theta, Theta_d;
	int dt;
	unsigned long t;
	unsigned long t_prev = 0;
	int val_prev =0;
	float V;
	float val;

	m_fEncoderValue = m_AMS_AS5048B->angleR(U_DEG);
	t = millis();
	dt = (t - m_t_prev);				// Time step
	Theta = m_fEncoderValue;		// Theta = Actual Angular Position of the Motor
	Theta_d = m_fTargetPosition;	// Theta_d = Desired Angular Position of the Motor

	m_e = Theta_d - Theta;			// Error
	m_inte = m_inte_prev + (dt * (m_e + m_e_prev) / 2);	// Integration of Error
	
	V = kp * m_e + ki * m_inte + (kd * (m_e - m_e_prev) / dt) ; // Controlling Function

	if (V > m_Vmax) {
		V = m_Vmax;
		m_inte = m_inte_prev;
	}

	if (V < m_Vmin) {
		V = m_Vmin;
		m_inte = m_inte_prev;
		m_fEncoderValue_prev=  m_fEncoderValue;
	}
/*
  WriteDriverVoltage(V, m_Vmax);
*/
	m_t_prev = t;
	m_inte_prev = m_inte;
	m_e_prev = m_e;

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