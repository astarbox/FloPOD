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
	PodController(motorCtrl *pMotionController);
	podStates getShutterState();
	float GetAzimuth();
	float getAltitude();
	void Abort();
	void Open();
	void Close();
	void SetParkAzimuth(float fAz);
	void GoToAzimuth(float fAz);
	void SyncPosition(float fAz);


private:
	motorCtrl	*mPodMotor = nullptr;
	float	m_fPartAzimuth = 0.0f;
	float	m_fAz = 0.0f;

};

PodController *podController = nullptr;

PodController::PodController(motorCtrl *pMotionController)
{
	// make sure we're not getting a nullptr
	if(pMotionController) {
		mPodMotor = pMotionController;
	}
}

podStates PodController::getShutterState()
{
	return IDLE;
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

}

void PodController::Open()
{
	if(mPodMotor)
		mPodMotor->Open();
}

void PodController::Close()
{
	if(mPodMotor)
		mPodMotor->Close();
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


#endif