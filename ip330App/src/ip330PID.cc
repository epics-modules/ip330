//Ip330PID.cc

/*
    Author: Mark Rivers
    04/08/03 MLR   Changed Ip330PID class to a derived class from the new
                   fastPID abstract base class.
                   Deleted configIp330PID function, since it really needs
                   doubles, and one can't pass doubles from the vxWorks command
                   line.  Also, all of this configuration information is sent by
                   device support at iocInit anyway.
    04/24/03 MLR   Changed so that if the period of Ip330PID is less than Ip330 it
                   averages readings rather than just ignorring intervening readings. 
*/

#include <stdio.h>
#include <string.h>

#include <epicsThread.h>
#include "Message.h"

#include "fastPID.h"
#include "Ip330.h"
#include "DAC128V.h"

class Ip330PID : public fastPID
{
public:
    double setMicroSecondsPerScan(double microSeconds);
    double getMicroSecondsPerScan();
    double readOutput();
    void writeOutput(double output);
    Ip330PID(Ip330 *pIp330, int ADCChannel, DAC128V *pDAC128V, int DACChannel);
private:
    static void callback(void*, int *data);   // Callback function from Ip330
    Ip330 *pIp330;
    int ADCChannel;
    DAC128V *pDAC128V;
    int DACChannel;
    int average;
    int numAverage;
    int accumulated;
};


// These C functions are provided so that we can create and configure the
// fastPID object from the vxWorks command line, which does not understand C++ syntax.
static char taskname[] = "ip330PID";
extern "C" int initIp330PID(const char *serverName, 
         Ip330 *pIp330, int ADCChannel, DAC128V *pDAC128V, int DACChannel,
         int queueSize)
{
    Ip330PID *pIp330PID = new Ip330PID(pIp330, ADCChannel, pDAC128V, DACChannel);
    fastPIDServer *pFastPIDServer = 
                          new fastPIDServer(serverName, pIp330PID, queueSize);

    epicsThreadId threadId = epicsThreadCreate(taskname,
                             epicsThreadPriorityMedium, 10000,
                             (EPICSTHREADFUNC)fastPIDServer::fastServer,
                             (void*) pFastPIDServer);
    if(threadId == NULL)
       errlogPrintf("%s ip330PIDServer ThreadCreate Failure\n",
            serverName);

    return(0);
}

Ip330PID::Ip330PID(Ip330 *pIp330, int ADCChannel, DAC128V *pDAC128V, int DACChannel)
: fastPID(),
  pIp330(pIp330), ADCChannel(ADCChannel), 
  pDAC128V(pDAC128V), DACChannel(DACChannel), 
  average(0), numAverage(1), accumulated(0)
{
  pIp330->registerCallback(callback, (void *)this);
}

void Ip330PID::callback(void *v, int *newData)
{
    Ip330PID *t = (Ip330PID *) v;

    // No need to average if collecting every point
    if (t->numAverage == 1) {
        t->doPID((double)newData[t->ADCChannel]);
       return;
    }
    t->average += newData[t->ADCChannel];
    if (++t->accumulated < t->numAverage) return;
    // We have now collected the desired number of points to average
    t->average /= t->accumulated;
    t->doPID((double)t->average);
    t->average = 0;
    t->accumulated = 0;
}

double Ip330PID::setMicroSecondsPerScan(double microSeconds)
{
    numAverage = (int) (microSeconds /
                        pIp330->getMicroSecondsPerScan() + 0.5);
    if (numAverage < 1) numAverage = 1;
    accumulated = 0;
    return getMicroSecondsPerScan();
}   

double Ip330PID::getMicroSecondsPerScan()
{
    return(pIp330->getMicroSecondsPerScan() * (numAverage));
}

double Ip330PID::readOutput()
{
    int currentDAC;
    pDAC128V->getValue(&currentDAC, DACChannel);
    return((double)currentDAC);
}

void Ip330PID::writeOutput(double output)
{
    pDAC128V->setValue((int)output, DACChannel);
}
