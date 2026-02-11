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
	PodController();
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
	motorMotion	mPodMotor;
	float	m_fPartAzimuth;
	float	m_fAz;

};

PodController *podController = nullptr;


#endif