//ip330Sweep.cc

/*
    Author: Mark Rivers
    10/27/99

    04/20/00  MLR  Changed timing logic.  No longer reprogram IP-330 clock,
                   just change whether the callback is executed when called.
    09/04/00  MLR  Changed presetTime to liveTime and realTime for
                   compatibility with devMcaMpf.
    01/16/01  MLR  Added check for valid pIp330 in ip330Sweep::init
    04/09/03  MLR  Converted to base class fastSweep, and this derived class.
    04/24/03  MLR  Changed logic so that if the sampling period is less than the
                   IP-330 clock period it averages, rather than subsamples.
                   Merged Ip330Sweep.h and ip330SweepServer.cc into this file.
    06/10/03  MLR  Converted to EPICS R3.14.2
*/

#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

#include <epicsTime.h>
#include <epicsThread.h>
#include <errlog.h>

#include "Message.h"

#include "fastSweep.h"
#include "Ip330.h"

class ip330Sweep : public fastSweep
{
public:
    ip330Sweep(Ip330 *pIp330, int firstChan, int lastChan, int maxPoints);
    double setMicroSecondsPerPoint(double microSeconds);
    double getMicroSecondsPerPoint();
private:
    static void callback(void*, int *data);   // Callback function from Ip330
    Ip330 *pIp330;
    int numAverage;
    int average[MAX_IP330_CHANNELS];
    int accumulated;
};

// This C function is needed because we call it from the vxWorks shell
static char taskname[] = "ip330Sweep";
extern "C" ip330Sweep *initIp330Sweep(
    Ip330 *pIp330, const char *serverName, int firstChan, int lastChan,
    int maxPoints, int queueSize)
{
    ip330Sweep *pIp330Sweep = new ip330Sweep(pIp330, firstChan, lastChan, maxPoints);
    fastSweepServer *pFastSweepServer = 
                     new fastSweepServer(serverName, pIp330Sweep, queueSize);

    epicsThreadId threadId = epicsThreadCreate(taskname,
                             epicsThreadPriorityMedium, 10000,
                             (EPICSTHREADFUNC)fastSweepServer::fastServer,
                             (void*) pFastSweepServer);
    if(threadId == NULL)
        errlogPrintf("%s ip330SweepServer ThreadCreate Failure\n",
            serverName);

    return(pIp330Sweep);
}

ip330Sweep::ip330Sweep(Ip330 *pIp330, int firstChan, int lastChan, int maxPoints) :
            fastSweep(firstChan, lastChan, maxPoints), pIp330(pIp330), 
            accumulated(0)
{
    int i;
    for (i=0; i<MAX_IP330_CHANNELS; i++) average[i]=0;
    pIp330->registerCallback(callback, (void *)this);
}

void ip330Sweep::callback(void *v, int *newData)
{
    ip330Sweep *t = (ip330Sweep *) v;
    int i;

    // No need to average if collecting every point
    if (t->numAverage == 1) {
       t->nextPoint(newData);
       return;
    }
    for (i=t->firstChan; i<t->lastChan; i++) t->average[i] += newData[i];
    if (++t->accumulated < t->numAverage) return;
    // We have now collected the desired number of points to average
    for (i=t->firstChan; i<t->lastChan; i++) t->average[i] /= t->accumulated;
    t->nextPoint(t->average);
    for (i=t->firstChan; i<t->lastChan; i++) t->average[i] = 0;
    t->accumulated = 0;
}

double ip330Sweep::setMicroSecondsPerPoint(double microSeconds)
{
    numAverage = (int) (microSeconds /
                        pIp330->getMicroSecondsPerScan() + 0.5);
    if (numAverage < 1) numAverage = 1;
    accumulated = 0;
    return getMicroSecondsPerPoint();
}   

double ip330Sweep::getMicroSecondsPerPoint()
{
    // Return dwell time in microseconds
    return(pIp330->getMicroSecondsPerScan() * (numAverage));
}
