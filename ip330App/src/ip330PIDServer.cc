//ip330PIDServer.cc

/********************COPYRIGHT NOTIFICATION**********************************
This software was developed under a United States Government license
described on the COPYRIGHT_UniversityOfChicago file included as part
of this distribution.
****************************************************************************/
/*
    Author: Mark Rivers
    Date: 10/27/99

*/

#include <vxWorks.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <taskLib.h>

#include "Message.h"
#include "Float64Message.h"
#include "Float64ArrayMessage.h"
#include "mpfType.h"
#include "Ip330PID.h"
#include "Ip330PIDServer.h"

extern "C"
{
#ifdef NODEBUG
#define DEBUG(l,f,v) ;
#else
#define DEBUG(l,f,v...) { if(l<Ip330PIDServerDebug) printf(f,## v); }
#endif
volatile int Ip330PIDServerDebug = 0;
}

class Ip330PIDServer {
public:
    Ip330PID *pIp330PID;
    MessageServer *pMessageServer;
    static void ip330Server(Ip330PIDServer *);
};


static char taskname[] = "ip330PID";
extern "C" Ip330PID *initIp330PID(const char *serverName, 
         Ip330 *pIp330, int ADCChannel, DAC128V *pDAC128V, int DACChannel,
         int queueSize)
{
    Ip330PID *pIp330PID = Ip330PID::init(pIp330, ADCChannel, 
         pDAC128V, DACChannel);

    if(!pIp330PID) return(0);
    Ip330PIDServer *pIp330PIDServer = new Ip330PIDServer;
    pIp330PIDServer->pIp330PID = pIp330PID;
    pIp330PIDServer->pMessageServer = new MessageServer(serverName,queueSize);
    int taskId = taskSpawn(taskname,100,VX_FP_TASK,2000,
        (FUNCPTR)Ip330PIDServer::ip330Server,(int)pIp330PIDServer,
        0,0,0,0,0,0,0,0,0);
    if(taskId==ERROR)
        printf("%s ip330Server taskSpawn Failure\n",
            pIp330PIDServer->pMessageServer->getName());
    return(pIp330PID);
}

extern "C" int configIp330PID(Ip330PID *pIp330PID, 
         double KP, double KI, double KD, 
         int interval, int feedbackOn, int lowLimit, int highLimit)
{
    if(!pIp330PID) return(-1);
    pIp330PID->config(pIp330PID, KP, KI, KD, interval, feedbackOn, 
                      lowLimit, highLimit);
    return(0);
}

void Ip330PIDServer::ip330Server(Ip330PIDServer *pIp330PIDServer)
{
    while(true) {
        MessageServer *pMessageServer = pIp330PIDServer->pMessageServer;
        Ip330PID *pIp330PID = pIp330PIDServer->pIp330PID;
        pMessageServer->waitForMessage();
        Message *inmsg;
        int cmd;
        double value;
        
        while((inmsg = pMessageServer->receive())) {
            Float64Message *pim = (Float64Message *)inmsg;
            Float64ArrayMessage *pam = NULL;
            if (inmsg->getType() != messageTypeFloat64) {
                printf("%s ip330Server got illegal message type %d\n",
                    pMessageServer->getName(), inmsg->getType());
                delete inmsg;
                break;
            }
            pim->status = 0;
            cmd = pim->cmd;
            value = pim->value;
            if ((Ip330PIDServerDebug > 0) && (cmd != cmdGetParams))
                printf("Ip330PIDServer, cmd=%d, value=%f\n", cmd, value);
            if ((Ip330PIDServerDebug > 5) && (cmd == cmdGetParams))
                printf("Ip330PIDServer, cmd=%d, value=%f\n", cmd, value);
            switch (cmd) {
            case cmdStartFeedback:
               pIp330PID->feedbackOn = 1;
               break;
            case cmdStopFeedback:
               pIp330PID->feedbackOn = 0;
               break;
            case cmdSetLowLimit:
               pIp330PID->lowLimit = (int)value;
               break;
            case cmdSetHighLimit:
               pIp330PID->highLimit = (int)value;
               break;
            case cmdSetKP:
               pIp330PID->KP = value;
               break;
            case cmdSetKI:
               pIp330PID->KI = value;
               break;
            case cmdSetKD:
               pIp330PID->KD = value;
               break;
            case cmdSetI:
               pIp330PID->I = value;
               break;
            case cmdSetSecondsPerScan:
               pIp330PID->setMicroSecondsPerScan(value*1.e6);
               break;
            case cmdSetSetPoint:
               pIp330PID->setPoint = (int)value;
               break;
            case cmdGetParams:
               pam = (Float64ArrayMessage *)pMessageServer->allocReplyMessage(
                                          pim, messageTypeFloat64Array);
               delete pim;
               pam->allocValue(numIp330PIDOffsets);
               pam->setLength(numIp330PIDOffsets);
               pam->value[offsetActual] = pIp330PID->actual;
               pam->value[offsetError] = pIp330PID->error;
               pam->value[offsetP] = pIp330PID->P;
               pam->value[offsetI] = pIp330PID->I;
               pam->value[offsetD] = pIp330PID->D;
               pam->value[offsetOutput] = pIp330PID->output;
               pam->value[offsetSecondsPerScan] =
                                 pIp330PID->getMicroSecondsPerScan()*1.e-6;
               break;
            default:
               printf("%s ip330Server got illegal command %d\n",
               pMessageServer->getName(), pim->cmd);
               break;
            }
            if (cmd == cmdGetParams)
               pMessageServer->reply(pam);
            else
               pMessageServer->reply(pim);
        }
    }
}
