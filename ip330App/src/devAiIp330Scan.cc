// devAiIp330Scan.cc

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
#include "aiRecord.h"
#include "recSup.h"
}

#include "Message.h"
#include "Int32Message.h"
#include "DevMpf.h"

class DevAiIp330Scan : public DevMpf
{
public:
        DevAiIp330Scan(dbCommon*,DBLINK*);
        long startIO(dbCommon* pr);
        long completeIO(dbCommon* pr,Message* m);
	long convert(dbCommon* pr,int pass);
	static long dev_init(void*);
private:
	int channel;
	int gain;
};

MAKE_LINCONV_DSET(devAiIp330Scan,DevAiIp330Scan::dev_init)

DevAiIp330Scan::DevAiIp330Scan(dbCommon* pr,DBLINK* l) : DevMpf(pr,l,false)
{
    vmeio* io = (vmeio*)&(l->value);
    gain = 0;
    channel=io->signal;
    aiRecord* prec=(aiRecord*)pr;
    const char* up = getUserParm();
    if(up && strlen(up)>0) {
        // decode the parm field, format: "gain"
        if((sscanf(up,"%d",&gain)<1) || gain<0 || gain>3) {
	    epicsPrintf("%s Illegal INP parm field\n", prec->name);
	    prec->pact=TRUE;
        }
    }
    convert((dbCommon *)prec,1);
    return;
}

long DevAiIp330Scan::startIO(dbCommon* )
{
    Int32Message *message = new Int32Message;
    message->value =gain;
    message->address =channel;
    return(sendReply(message));
}

long DevAiIp330Scan::completeIO(dbCommon* pr,Message* m)
{
    aiRecord* pai = (aiRecord*)pr;
    if((m->getType()) != messageTypeInt32) {
	epicsPrintf("%s ::completeIO illegal message type %d\n",
		    pai->name,m->getType());
        recGblSetSevr(pai,READ_ALARM,INVALID_ALARM);
        delete m;
        return(MPF_NoConvert);
    }
    Int32Message *pint32Message = (Int32Message *)m;
    if(pint32Message->status) {
        recGblSetSevr(pai,READ_ALARM,INVALID_ALARM);
        delete m;
        return(MPF_NoConvert);
    }
    pai->rval=pint32Message->value;
    pai->udf=0;
    delete m;
    return(MPF_OK);
}

long DevAiIp330Scan::convert(dbCommon* pr,int /*pass*/)
{
    aiRecord* pai = (aiRecord*)pr;
    pai->eslo=(pai->eguf-pai->egul)/(double)0xffff;
    return 0;
}

long DevAiIp330Scan::dev_init(void* v)
{
	aiRecord* pr = (aiRecord*)v;
	DevAiIp330Scan* pDevAiIp330Scan
            = new DevAiIp330Scan((dbCommon*)pr,&(pr->inp));
      pDevAiIp330Scan->bind();
	return pDevAiIp330Scan->getStatus();
}

