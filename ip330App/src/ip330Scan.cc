//ip330Scan.cc

/********************COPYRIGHT NOTIFICATION**********************************
This software was developed under a United States Government license
described on the COPYRIGHT_UniversityOfChicago file included as part
of this distribution.
****************************************************************************/
/*
    Original Author: Jim Kowalkowski
    Date: 2/15/95
    Current Authors: Mark Rivers, Joe Sullivan and Marty Kraimer
    Converted to MPF: 12/9/98
    10/27/99 MLR Converted to use ip330 (base class): 
    04/20/00 MLR Changed logic for timing, no longer support changing clock
                 rate of Ip330.
    01/16/01 MLR Added check for valid pIp330 in Ip330Scan::init
    03/31/03 MLR Made time to average be independent for each channel, previously
                 all channels used the same time. Added some debugging.
    04/24/03 MLR Changed so that there is no "time to average" concept.  Rather
                 averaging is done until the value is read with getValue().
                 Merged Ip330Scan.h and Ip330ScanServer.cc into this file.
*/

#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

#include <epicsThread.h>
#include <iocsh.h>
#include <epicsTypes.h>
#include <epicsExport.h>
#include "symTable.h"

#include "Message.h"
#include "Int32Message.h"
#include "mpfType.h"

#include "Ip330.h"

class Ip330ScanData
{
public:
    long averageStore;
    int accumulated;
};

class Ip330Scan
{
public:
    Ip330Scan(Ip330 *pIp330, int firstChan, int lastChan);
    int getValue(int channel);
    int setGain(int gain,int channel);
private:
    static void callback(void*, int *data);   // Callback function from Ip330
    Ip330 *pIp330;
    int firstChan;
    int lastChan;
    Ip330ScanData *chanData;
};

class Ip330ScanServer {
public:
    Ip330Scan *pIp330Scan;
    MessageServer *pMessageServer;
    static void ip330Server(Ip330ScanServer *);
};

extern "C"
{
#ifdef NODEBUG
#define DEBUG(l,f,v) ;
#else
#define DEBUG(l,f,v...) { if(l<Ip330ScanDebug) printf(f,## v); }
#endif
volatile int Ip330ScanDebug = 0;
}


static char taskname[] = "ip330Scan";
extern "C" int initIp330Scan(const char *ip330Name, const char *serverName, 
                             int firstChan, int lastChan, int queueSize)
{
    Ip330 *pIp330 = Ip330::findModule(ip330Name);
    if (pIp330 == NULL) {
       printf("initIp330Scan: cannot find IP330 module %s\n", ip330Name);
       return(-1);
    }
    Ip330Scan *pIp330Scan = new Ip330Scan(pIp330, firstChan, lastChan);
    Ip330ScanServer *pIp330ScanServer = new Ip330ScanServer;
    pIp330ScanServer->pIp330Scan = pIp330Scan;
    pIp330ScanServer->pMessageServer = new MessageServer(serverName,queueSize);

    epicsThreadId threadId = epicsThreadCreate(taskname,
                             epicsThreadPriorityMedium, 10000,
                             (EPICSTHREADFUNC)Ip330ScanServer::ip330Server,
                             (void*) pIp330ScanServer);
    if(threadId == NULL)
        errlogPrintf("%s ip330ScanServer ThreadCreate Failure\n",
            serverName);

    return(0);
}


Ip330Scan:: Ip330Scan(
    Ip330 *pIp330, int firstChan, int lastChan)
: pIp330(pIp330), firstChan(firstChan), lastChan(lastChan)
{
    int chan;
    chanData = (Ip330ScanData *)calloc(MAX_IP330_CHANNELS,sizeof(Ip330ScanData));
    if(!chanData) {
        printf("Ip330Scan calloc failed\n");
        return;
    }
    for (chan=0; chan<MAX_IP330_CHANNELS; chan++) {
       chanData[chan].accumulated = 0;
    }
    pIp330->registerCallback(callback, (void *)this);
}

int Ip330Scan::getValue(int channel)
{
    Ip330ScanData  *sd = &chanData[channel];
    long average;
    
    DEBUG(5, "Ip330Scan:getValue, chan=%d, accumulated=%d, averageStore=%ld\n",
                               channel,
                               sd->accumulated, 
                               sd->averageStore);
    if (sd->accumulated == 0) sd->accumulated = 1;
    average = sd->averageStore / sd->accumulated;
    sd->averageStore = 0;
    sd->accumulated = 0;
    return(pIp330->correctValue(channel, average) );
}

void Ip330Scan:: callback(void *v, int *newData)
{
    Ip330Scan      *t = (Ip330Scan *) v;
    int             i;
    Ip330ScanData  *sd;

    for (i = t->firstChan; i <= t->lastChan; i++) {
        sd = &t->chanData[i];
        sd->accumulated++;
        sd->averageStore += newData[i];
    }
}

int Ip330Scan:: setGain(int gain, int channel)
{
    pIp330->setGain(gain, channel);
    return(0);
}


void Ip330ScanServer::ip330Server(Ip330ScanServer *pIp330ScanServer)
{
    while(true) {
       MessageServer *pMessageServer = pIp330ScanServer->pMessageServer;
       Ip330Scan *pIp330Scan = pIp330ScanServer->pIp330Scan;
       pMessageServer->waitForMessage();
       Message *inmsg;
       while((inmsg = pMessageServer->receive())) {
          if(inmsg->getType()!=messageTypeInt32) {
             printf("%s ip330Server got illegal message type %d\n",
                pMessageServer->getName(), inmsg->getType());
             delete inmsg;
          } else {
             Int32Message *pmessage = (Int32Message *)inmsg;
             pmessage->status = 0;
             int gain = pmessage->value;
             int channel = pmessage->address;
             if (pIp330Scan->setGain(gain,channel)) pmessage->status= -1;
             if (pmessage->status==0) {
                pmessage->value = (int32)pIp330Scan->getValue(channel);
             }
             pMessageServer->reply(pmessage);
          }
       }
    }
}

static const iocshArg scanArg0 = { "ip330Name",iocshArgString};
static const iocshArg scanArg1 = { "serverName",iocshArgString};
static const iocshArg scanArg2 = { "firstChan",iocshArgInt};
static const iocshArg scanArg3 = { "lastChan",iocshArgInt};
static const iocshArg scanArg4 = { "queueSize",iocshArgInt};
static const iocshArg * scanArgs[5] = {&scanArg0,
                                       &scanArg1,
                                       &scanArg2,
                                       &scanArg3,
                                       &scanArg4};
static const iocshFuncDef scanFuncDef = {"initIp330Scan",5,scanArgs};
static void scanCallFunc(const iocshArgBuf *args)
{
    initIp330Scan(args[0].sval, args[1].sval, args[2].ival,
                  args[3].ival, args[4].ival);
}
void ip330ScanRegister(void)
{
    addSymbol("Ip330ScanDebug", (epicsInt32 *)&Ip330ScanDebug, epicsInt32T);
    iocshRegister(&scanFuncDef,scanCallFunc);
}

epicsExportRegistrar(ip330ScanRegister);
