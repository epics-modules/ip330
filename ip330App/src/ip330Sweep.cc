//ip330Sweep.cc

/********************COPYRIGHT NOTIFICATION**********************************
This software was developed under a United States Government license
described on the COPYRIGHT_UniversityOfChicago file included as part
of this distribution.
****************************************************************************/
/*
    Author: Mark Rivers
    10/27/99

    04/20/00  MLR  Changed timing logic.  No longer reprogram IP-330 clock,
                   just change whether the callback is executed when called.
    09/04/00  MLR  Changed presetTime to liveTime and realTime for
                   compatibility with devMcaMpf.
    01/16/01 MLR Added check for valid pIp330 in Ip330Sweep::init
*/

#include <vxWorks.h>
#include <iv.h>
#include <stdlib.h>
#include <sysLib.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

#include <tickLib.h>

#include "Ip330Sweep.h"

extern "C"
{
#ifdef NODEBUG
#define DEBUG(l,f,v) ;
#else
#define DEBUG(l,f,v...) { if(l<Ip330SweepDebug) printf(f,## v); }
#endif
volatile int Ip330SweepDebug = 0;
}



Ip330Sweep * Ip330Sweep::init(Ip330 *pIp330, int firstChan, int lastChan,
                            int maxPoints)
{
    if (!pIp330) return(0);
    Ip330Sweep *pIp330Sweep = new Ip330Sweep(pIp330,firstChan,lastChan,
                                             maxPoints);
    return(pIp330Sweep);
}

Ip330Sweep:: Ip330Sweep(
    Ip330 *pIp330, int firstChan, int lastChan, int maxPoints)
: pIp330(pIp330), firstChan(firstChan), lastChan(lastChan),
  acquiring(0), numAcquired(0), maxPoints(maxPoints), 
  elapsedTime(0), startTime(0), 
  numChans(lastChan - firstChan + 1), numPoints(maxPoints)
{
    DEBUG(1, "Ip330Sweep:Ip330Sweep, maxPoints=%d, numChans=%d\n", 
      maxPoints, numChans);
    pData = (int *)calloc(maxPoints*numChans,sizeof(int));
    if(!pData) {
        printf("Ip330Sweep calloc failed\n");
        return;
    }
    pIp330->registerCallback(callback, (void *)this);
}

void Ip330Sweep::getData(int channel, int *data)
{
    memcpy(data, &pData[maxPoints*(channel-firstChan)], numPoints*sizeof(int));
}

void Ip330Sweep:: callback(void *v, int *newData)
{
    Ip330Sweep *t = (Ip330Sweep *) v;
    int i;

    if (++t->skip <= t->numSkip) return;
    t->skip = 0;
    
    if (t->numAcquired >= t->numPoints) {
       t->acquiring = 0;
    }
    if (!t->acquiring) return;
    
    int offset = t->numAcquired;
    for (i = 0; i < (t->numChans); i++) {
        t->pData[offset] = newData[t->firstChan + i];
        offset += t->maxPoints;
    }
    t->numAcquired++;
    t->elapsedTime = tickGet() - t->startTime;
    if ((t->realTime > 0) && (t->elapsedTime >= t->realTime))
            t->acquiring = 0;
    if ((t->liveTime > 0) && (t->elapsedTime >= t->liveTime))
            t->acquiring = 0;
}

int Ip330Sweep:: setGain(int gain, int channel)
{
    pIp330->setGain(gain, channel);
    return(0);
}

void Ip330Sweep:: erase()
{
    memset(pData, 0, maxPoints*numChans*sizeof(int));
    numAcquired = 0;
    /* Reset the elapsed time */
    elapsedTime = 0;
    startTime = tickGet();
}   

void Ip330Sweep:: startAcquire()
{
    if (!acquiring) {
       acquiring = 1;
       startTime = tickGet();
    }
}   

void Ip330Sweep:: stopAcquire()
{
    acquiring = 0;
}   

int Ip330Sweep:: setNumPoints(int n)
{
    if ((n < 1) || (n > maxPoints)) return(-1);
    numPoints = n;
    return(0);
}   

int Ip330Sweep:: getNumPoints()
{
    return(numPoints);
}   

int Ip330Sweep:: setRealTime(float time)
{
    // "time" is in seconds, convert to clock ticks
    realTime = (int) (time*sysClkRateGet() + 0.5);
    return(0);
}   

int Ip330Sweep:: setLiveTime(float time)
{
    // "time" is in seconds, convert to clock ticks
    liveTime = (int) (time*sysClkRateGet() + 0.5);
    return(0);
}   

float Ip330Sweep:: getElapsedTime()
{
    // Return elapsed time in seconds, convert from clock ticks
    return( (float)elapsedTime / (float)sysClkRateGet());
}

int Ip330Sweep:: getStatus()
{
    return(acquiring);
}

float Ip330Sweep:: setMicroSecondsPerPoint(float microSeconds)
{
    numSkip = (int) (microSeconds /
                        pIp330->getMicroSecondsPerScan() + 0.5) - 1;
    if (numSkip < 0) numSkip = 0;
    skip = 0;
    return getMicroSecondsPerPoint();
}   

float Ip330Sweep:: getMicroSecondsPerPoint()
{
    // Return dwell time in microseconds
    return(pIp330->getMicroSecondsPerScan() * (numSkip+1));
}
