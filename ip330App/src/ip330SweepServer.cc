//ip330SweepServer.cc

/********************COPYRIGHT NOTIFICATION**********************************
This software was developed under a United States Government license
described on the COPYRIGHT_UniversityOfChicago file included as part
of this distribution.
****************************************************************************/
/*
    Author: Mark Rivers
    Date: 10/27/99

    Modified:
    04-Sept-2000  MLR  Rewrote to communicate with generic devMcaMpf.cc

*/

#include <vxWorks.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <taskLib.h>

#include "Message.h"
#include "Float64Message.h"
#include "Int32ArrayMessage.h"
#include "Float64ArrayMessage.h"
#include "mpfType.h"
#include "mca.h"
#include "Ip330Sweep.h"
#include "DevMcaMpf.h"

extern "C"
{
#ifdef NODEBUG
#define DEBUG(l,f,v) ;
#else
#define DEBUG(l,f,v...) { if(l<Ip330SweepServerDebug) printf(f,## v); }
#endif
volatile int Ip330SweepServerDebug = 0;
}


class Ip330SweepServer {
public:
    Ip330Sweep *pIp330Sweep;
    MessageServer *pMessageServer;
    static void ip330Server(Ip330SweepServer *);
};


static char taskname[] = "ip330Sweep";
extern "C" int initIp330Sweep(
    Ip330 *pIp330, const char *serverName, int firstChan, int lastChan,
    int maxPoints, int queueSize)
{
    Ip330Sweep *pIp330Sweep = Ip330Sweep::init(pIp330, firstChan, lastChan,
                                            maxPoints);
    if(!pIp330Sweep) return(0);
    Ip330SweepServer *pIp330SweepServer = new Ip330SweepServer;
    pIp330SweepServer->pIp330Sweep = pIp330Sweep;
    pIp330SweepServer->pMessageServer = new MessageServer(serverName,queueSize);
    int taskId = taskSpawn(taskname,100,VX_FP_TASK,2000,
        (FUNCPTR)Ip330SweepServer::ip330Server,(int)pIp330SweepServer,
        0,0,0,0,0,0,0,0,0);
    if(taskId==ERROR)
        printf("%s ip330Server taskSpawn Failure\n",
            pIp330SweepServer->pMessageServer->getName());
    return(0);
}

void Ip330SweepServer::ip330Server(Ip330SweepServer *pIp330SweepServer)
{
    while(true) {
        MessageServer *pMessageServer = pIp330SweepServer->pMessageServer;
        Ip330Sweep *pIp330Sweep = pIp330SweepServer->pIp330Sweep;
        pMessageServer->waitForMessage();
        Message *inmsg;
        int cmd;
        int nchans=0;
        int len;
        struct devMcaMpfStatus *pstat;

        while((inmsg = pMessageServer->receive())) {
            Float64Message *pim = (Float64Message *)inmsg;
            Int32ArrayMessage *pi32m = NULL;
            Float64ArrayMessage *pf64m = NULL;
            pim->status = 0;
            cmd = pim->cmd;
            DEBUG(2, "(Ip330SweepServer): Got Message, cmd=%d\n", cmd)
            switch (cmd) {
               case MSG_ACQUIRE:
                  pIp330Sweep->startAcquire();
                  break;
               case MSG_READ:
                  pi32m = (Int32ArrayMessage *)pMessageServer->
                           allocReplyMessage(pim, messageTypeInt32Array);
                  nchans = pIp330Sweep->getNumPoints();
                  DEBUG(2, "cmdRead, allocating nchans=%d\n", nchans)
                  pi32m->allocValue(nchans);
                  DEBUG(2, "cmdRead, setLength\n")
                  pi32m->setLength(nchans);
                  DEBUG(2, "cmdRead, getData, address=%d\n", pim->address)
                  pIp330Sweep->getData(pim->address, pi32m->value);
                  delete pim;
                  pMessageServer->reply(pi32m);
                  break;
               case MSG_SET_CHAS_INT:
                  // No-op for Ip330Sweep
                  break;
               case MSG_SET_CHAS_EXT:
                  // No-op for Ip330Sweep
                  break;
               case MSG_SET_NCHAN:
                  if(pIp330Sweep->setNumPoints(pim->value))
                        pim->status= -1;
                  break;
               case MSG_SET_DWELL:
                  if(pIp330Sweep->setMicroSecondsPerPoint(pim->value*1.e6))
                        pim->status= -1;
                  break;
               case MSG_SET_REAL_TIME:
                  if(pIp330Sweep->setRealTime(pim->value))
                        pim->status= -1;
                  break;
               case MSG_SET_LIVE_TIME:
                  if(pIp330Sweep->setLiveTime(pim->value))
                        pim->status= -1;
                  break;
               case MSG_SET_COUNTS:
                  // No-op for Ip330Sweep
                  break;
               case MSG_SET_LO_CHAN:
                  // No-op for Ip330Sweep
                  break;
               case MSG_SET_HI_CHAN:
                  // No-op for Ip330Sweep
                  break;
               case MSG_SET_NSWEEPS:
                  // No-op for Ip330Sweep
                  break;
               case MSG_SET_MODE_PHA:
                  // No-op for Ip330Sweep
                  break;
               case MSG_SET_MODE_MCS:
                  // No-op for Ip330Sweep
                  break;
               case MSG_SET_MODE_LIST:
                  // No-op for Ip330Sweep
                  break;
               case MSG_GET_ACQ_STATUS:
                  pf64m = (Float64ArrayMessage *)pMessageServer->
                                allocReplyMessage(pim, messageTypeFloat64Array);
                  len = sizeof(struct devMcaMpfStatus) / sizeof(double);
                  DEBUG(2, "(Ip330SweepServer): len=%d\n", len)
                  pf64m->allocValue(len);
                  pf64m->setLength(len);
                  pstat = (struct devMcaMpfStatus *)pf64m->value;
                  pstat->realTime = pIp330Sweep->getElapsedTime();
                  pstat->liveTime = pIp330Sweep->getElapsedTime();
                  pstat->dwellTime = 
                     pIp330Sweep->getMicroSecondsPerPoint()/1.e6;
                  pstat->acquiring = pIp330Sweep->getStatus();
                  pstat->totalCounts = 0;
                  delete pim;
                  pMessageServer->reply(pf64m);
                  break;
               case MSG_STOP_ACQUISITION:
                  pIp330Sweep->stopAcquire();
                  break;
               case MSG_ERASE:
                  pIp330Sweep->erase();
                  break;
               case MSG_SET_SEQ:
                  // No-op for Ip330Sweep
                  break;
               case MSG_SET_PSCL:
                  // No-op for Ip330Sweep
                  break;
               default:
                  printf("%s ip330Server got illegal command %d\n",
                     pMessageServer->getName(), pim->cmd);
                  break;
            }
            if (cmd != MSG_READ && cmd != MSG_GET_ACQ_STATUS)
               pMessageServer->reply(pim);
        }
    }
}
