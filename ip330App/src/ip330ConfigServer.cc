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

#include <vxWorks.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <taskLib.h>

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
    Ip330 *pIp330, const char *serverName, int queueSize)
{
    if (!pIp330) return(0);
    Ip330ConfigServer *pIp330ConfigServer = new Ip330ConfigServer;
    pIp330ConfigServer->pIp330 = pIp330;
    pIp330ConfigServer->pMessageServer = new MessageServer(serverName,queueSize);
    int taskId = taskSpawn(taskname,100,VX_FP_TASK,4000,
        (FUNCPTR)Ip330ConfigServer::ip330Server,(int)pIp330ConfigServer,
        0,0,0,0,0,0,0,0,0);
    if(taskId==ERROR)
        printf("%s ip330Server taskSpawn Failure\n",
            pIp330ConfigServer->pMessageServer->getName());
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
