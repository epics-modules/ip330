//ip330ConfigServer.cc

/********************COPYRIGHT NOTIFICATION**********************************
This software was developed under a United States Government license
described on the COPYRIGHT_UniversityOfChicago file included as part
of this distribution.
****************************************************************************/
/*
    Original Author: Marty Kraimer
    Date: 31OCT2000
    Current Authors: Mark Rivers, Joe Sullivan, and Marty Kraimer

    01/16/01 MLR Added check for valid pIp330 in initIp330Config()

*/

#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <taskLib.h>

#include <epicsThread.h>
#include <iocsh.h>
#include <epicsExport.h>

#include "Message.h"
#include "WatchDog.h"
#include "Int32Message.h"
#include "mpfType.h"
#include "Ip330.h"
#include "Ip330ConfigServer.h"

class Ip330ConfigServer {
public:
    Ip330 *pIp330;
    MessageServer *pMessageServer;
    static void ip330Server(Ip330ConfigServer *);
};


static char taskname[] = "ip330Config";
extern "C" int initIp330Config(
    const char *ip330Name, const char *serverName, int queueSize)
{
    Ip330 *pIp330 = Ip330::findModule(ip330Name);
    if (pIp330 == NULL) {
       printf("initIp330Config: cannot find IP330 module %s\n", ip330Name);
       return(-1);
    }
    Ip330ConfigServer *pIp330ConfigServer = new Ip330ConfigServer;
    pIp330ConfigServer->pIp330 = pIp330;
    pIp330ConfigServer->pMessageServer = new MessageServer(serverName,queueSize);

    epicsThreadId threadId = epicsThreadCreate(taskname,
                             epicsThreadPriorityMedium, 10000,
                             (EPICSTHREADFUNC)Ip330ConfigServer::ip330Server,
                             (void*) pIp330ConfigServer);
    if(threadId == NULL)
        errlogPrintf("%s ip330ConfigServer ThreadCreate Failure\n",
            serverName);
    return(0);
}

void Ip330ConfigServer::ip330Server(Ip330ConfigServer *pIp330ConfigServer)
{
    while(true) {
        MessageServer *pMessageServer = pIp330ConfigServer->pMessageServer;
        Ip330 *pIp330 = pIp330ConfigServer->pIp330;
        pMessageServer->waitForMessage();
        Message *inmsg;
        while((inmsg = pMessageServer->receive())) {
            if(inmsg->getType()!=messageTypeInt32) {
                printf("%s ip330ConfigServer got illegal message type %d\n",
                    pMessageServer->getName(), inmsg->getType());
                delete inmsg;
            } else {
                Int32Message *pmessage = (Int32Message *)inmsg;
                pmessage->status = 0;
                switch(pmessage->cmd) {
                case ip330SetSecondsBetweenCalibrate:
                    pIp330->setSecondsBetweenCalibrate(pmessage->value);
                    break;
                default:
                    pmessage->status= -1;
                    printf("%s ip330ConfigServer got illegal cmd %d\n",
                        pMessageServer->getName(), pmessage->cmd);
                }
                pMessageServer->reply(pmessage);
            }
        }
    }
}

static const iocshArg configArg0 = { "ip330Name",iocshArgString};
static const iocshArg configArg1 = { "serverName",iocshArgString};
static const iocshArg configArg2 = { "queueSize",iocshArgInt};
static const iocshArg * configArgs[3] = {&configArg0,
                                        &configArg1,
                                        &configArg2};
static const iocshFuncDef configFuncDef = {"initIp330Config",3,configArgs};
static void configCallFunc(const iocshArgBuf *args)
{
    initIp330Config(args[0].sval, args[1].sval, (int) args[2].sval);
}

void ip330ConfigRegister(void)
{
    iocshRegister(&configFuncDef,configCallFunc);
}

epicsExportRegistrar(ip330ConfigRegister);


