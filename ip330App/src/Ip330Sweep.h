//Ip330Sweep.h

/********************COPYRIGHT NOTIFICATION**********************************
This software was developed under a United States Government license
described on the COPYRIGHT_UniversityOfChicago file included as part
of this distribution.
****************************************************************************/
/*
    Author: Mark Rivers
    Date: 10/27/99
  
    04/20/00  MLR  Added private members for timing

*/

#ifndef ip330SweepH
#define ip330SweepH

#include "Ip330.h"

class Ip330Sweep
{
public:
    static Ip330Sweep * init(Ip330 *pIp330, int firstChan, int lastChan, 
    	                    int maxPoints);
    int setGain(int gain,int channel);
    void startAcquire();
    void stopAcquire();
    void erase();
    float setMicroSecondsPerPoint(float microSeconds);
    float getMicroSecondsPerPoint();
    int setNumPoints(int numPoints);
    int getNumPoints();
    int setRealTime(float time);
    int setLiveTime(float time);
    float getElapsedTime();
    int getStatus();
    void getData(int channel, int *data);
private:
    Ip330Sweep(Ip330 *pIp330, int firstChan, int lastChan, int maxPoints);
    static void callback(void*, int *data);   // Callback function from Ip330
    Ip330 *pIp330;
    int firstChan;
    int lastChan;
    int acquiring;
    int numAcquired;
    int maxPoints;
    int realTime;
    int liveTime;
    int elapsedTime;
    int startTime;
    int numChans;
    int numPoints;
    int *pData;
    int numSkip;
    int skip;
};

#endif //ip330SweepH
