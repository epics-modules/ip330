//ip330ScanServer.cc

/********************COPYRIGHT NOTIFICATION**********************************
This software was developed under a United States Government license
described on the COPYRIGHT_UniversityOfChicago file included as part
of this distribution.
****************************************************************************/
/*
    Original Author: Jim Kowalkowski
    Date: 2/15/95
    Current Authors: Mark Rivers, Joe Sullivan, and Marty Kraimer
    Converted to MPF: 12/9/98
    Converted to use Ip330 class: 10/27/99

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
#include "Ip330Scan.h"

class Ip330ScanServer {
public:
    Ip330Scan *pIp330Scan;
    MessageServer *pMessageServer;
    static void ip330Server(Ip330ScanServer *);
};


static char taskname[] = "ip330Scan";
extern "C" int initIp330Scan(
    Ip330 *pIp330, const char *serverName, int firstChan, int lastChan, 
    int milliSecondsToAverage, int queueSize)
{
    Ip330Scan *pIp330Scan = Ip330Scan::init(pIp330, firstChan, lastChan, 
                                            milliSecondsToAverage);
    if(!pIp330Scan) return(0);
    Ip330ScanServer *pIp330ScanServer = new Ip330ScanServer;
    pIp330ScanServer->pIp330Scan = pIp330Scan;
    pIp330ScanServer->pMessageServer = new MessageServer(serverName,queueSize);
    int taskId = taskSpawn(taskname,100,VX_FP_TASK,2000,
        (FUNCPTR)Ip330ScanServer::ip330Server,(int)pIp330ScanServer,
        0,0,0,0,0,0,0,0,0);
    if(taskId==ERROR)
        printf("%s ip330Server taskSpawn Failure\n",
            pIp330ScanServer->pMessageServer->getName());
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
                if(pIp330Scan->setGain( gain,channel)) pmessage->status= -1;
                if(pmessage->status==0) {
                    pmessage->value = (int32)pIp330Scan->getValue(channel);
                }
                pMessageServer->reply(pmessage);
            }
        }
    }
}
