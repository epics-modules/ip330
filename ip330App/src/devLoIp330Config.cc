// devLoIp330Config.cc

/********************COPYRIGHT NOTIFICATION**********************************
This software was developed under a United States Government license
described on the COPYRIGHT_UniversityOfChicago file included as part
of this distribution.
****************************************************************************/
/*
    Original Author: Jim Kowalkowski
    Date: 2/15/95

    Current Author: Joe Sullivan and Marty Kraimer
    Converted to MPF: 12/9/98

*/


#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
 
extern "C" {
#include "dbAccess.h"
#include "dbDefs.h"
#include "link.h"
#include "epicsPrint.h"
#include "dbCommon.h"
#include "longoutRecord.h"
#include "recSup.h"
}

#include "Message.h"
#include "Int32Message.h"
#include "DevMpf.h"
#include "Ip330ConfigServer.h"

class DevLoIp330Config : public DevMpf
{
public:
        DevLoIp330Config(dbCommon*,DBLINK*);
        long startIO(dbCommon* pr);
        long completeIO(dbCommon* pr,Message* m);
	static long dev_init(void*);
private:
	int cmd;
};

MAKE_DSET(devLoIp330Config,DevLoIp330Config::dev_init)

DevLoIp330Config::DevLoIp330Config(dbCommon* pr,DBLINK* l) : DevMpf(pr,l,false)
{
    cmd = 0;
    longoutRecord* prec=(longoutRecord*)pr;
    const char* up = getUserParm();
    if(up && strlen(up)>0) {
        // decode the parm field, format: "cmd"
        if((sscanf(up,"%d",&cmd)<1) || cmd<0 || cmd>0) {
	    epicsPrintf("%s Illegal INP parm field\n", prec->name);
	    prec->pact=TRUE;
        }
    }
    return;
}

long DevLoIp330Config::startIO(dbCommon* pr)
{
    longoutRecord* plo = (longoutRecord*)pr;
    Int32Message *message = new Int32Message;
    message->value = (int)plo->val;
    message->cmd =cmd;
    return(sendReply(message));
}

long DevLoIp330Config::completeIO(dbCommon* pr,Message* m)
{
    longoutRecord* plo = (longoutRecord*)pr;
    if((m->getType()) != messageTypeInt32) {
	epicsPrintf("%s ::completeIO illegal message type %d\n",
		    plo->name,m->getType());
        recGblSetSevr(plo,READ_ALARM,INVALID_ALARM);
        delete m;
        return(MPF_OK);
    }
    Int32Message *pint32Message = (Int32Message *)m;
    if(pint32Message->status) {
        recGblSetSevr(plo,READ_ALARM,INVALID_ALARM);
        delete m;
        return(MPF_OK);
    }
    plo->udf=0;
    delete m;
    return(MPF_OK);
}

long DevLoIp330Config::dev_init(void* v)
{
	longoutRecord* pr = (longoutRecord*)v;
	DevLoIp330Config* pDevLoIp330Config
            = new DevLoIp330Config((dbCommon*)pr,&(pr->out));
	return pDevLoIp330Config->getStatus();
}

