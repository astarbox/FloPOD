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
	void setIsCAlibrated(bool bCalibrated);

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
	// if the state is error, shut off power
	if(m_nState == POD_ERROR) {
		Abort();
		// mPowerController->setPortState(); -> apparently no way to cut the motor power, need to check schematics
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

}

void PodController::GoToAzimuth(float fAz)
{

}

void PodController::SyncPosition(float fAz)
{

}

bool PodController::isCalibrated()
{
	return m_isCalibrated;
}

void PodController::setIsCAlibrated(bool bCalibrated)
{
	m_isCalibrated = bCalibrated;
}

#endif