//
// FLO POD controller
// Copyright © 2026 AStarBox. All rights reserved.
//

#ifndef __POD_CTRL__
#define __POD_CTRL__
#include "config.h"
#include "motorCtrl.h"

volatile bool bParked = false;
//
// Some method are only used to satisfy Alpaca requirements.
//
class PodController
{
public:
	PodController(motorCtrl *pMotionController, powerPorts *pPowerController);
	podStates getShutterState();
	float GetAzimuth();
	float getAltitude();
	void Abort();
	void Open();
	void Close();
	void SetParkAzimuth(float fAz);
	void GoToAzimuth(float fAz);
	void SyncPosition(float fAz);
	bool isCalibrated();
	void setIsCalibrated(bool bCalibrated);

private:
	motorCtrl	*mPodMotor = nullptr;
	powerPorts	*mPowerController = nullptr;
	float		m_fPartAzimuth = 0.0f;
	float		m_fAz = 0.0f;
	podStates 	m_nState = IDLE;
	bool		m_isCalibrated = false;
};

PodController *podController = nullptr;

PodController::PodController(motorCtrl *pMotionController, powerPorts *pPowerController)
{
	// make sure we're not getting a nullptr
	if(pMotionController) {
		mPodMotor = pMotionController;
	}
	if(pPowerController) {
		mPowerController = pPowerController;
	}
}

podStates PodController::getShutterState()
{
	PodShutterState mPsState;
	// if the state is error, shut off power
	if(m_nState == POD_ERROR) {
		Abort();
	}
	mPodMotor->getShutterState(mPsState);
	switch(mPsState) {
		case PS_UNKNOWN:
			m_nState = POD_ERROR;
			break;
		case PS_CLOSED:
			m_nState = CLOSED;
			break;
		case  PS_OPEN:
			m_nState = OPEN;
			break;
		case PS_CLOSING:
			m_nState = CLOSING;
			break;
		case PS_OPENING:
			m_nState = OPENING;
			break;
		default:
			break;
		}

	return m_nState;
}

float PodController::GetAzimuth()
{
	return m_fAz;
}

float PodController::getAltitude()
{
	return 0.0f;
}

void PodController::Abort()
{
	mPodMotor->Stop();
}

void PodController::Open()
{
	if(mPodMotor) {
		m_nState = OPENING;
		mPodMotor->Open();
	}
}

void PodController::Close()
{
	if(mPodMotor) {
		m_nState = CLOSING;
		mPodMotor->Close();
	}
}

void PodController::SetParkAzimuth(float fAz)
{
	m_fAz = fAz;

}

void PodController::GoToAzimuth(float fAz)
{
	m_fAz = fAz;
}

void PodController::SyncPosition(float fAz)
{
	m_fAz = fAz;
}

bool PodController::isCalibrated()
{
	return m_isCalibrated;
}

void PodController::setIsCalibrated(bool bCalibrated)
{
	m_isCalibrated = bCalibrated;
}

#endif