/* devIp330.c

    Author: Mark Rivers
    Created:  April 21, 2003, based on devAiIp330Scan.cc
    July 7, 2004 MLR Converted from MPF and C++ to asyn and C

    Device support for Ip330. Records supported include:
    ai     - averages readings from Ip330
    longout - sends calibration interval in integer seconds

*/

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <dbAccess.h>
#include <dbDefs.h>
#include <link.h>
#include <dbCommon.h>
#include <aiRecord.h>
#include <longoutRecord.h>
#include <recSup.h>
#include <devSup.h>
#include <dbScan.h>
#include <cantProceed.h>
#include <epicsPrint.h>
#include <epicsExport.h>
#include <epicsMutex.h>
#include <asynDriver.h>
#include <asynInt32Callback.h>

#include <recGbl.h>
#include <alarm.h>

#include "drvIp330.h"


typedef enum {recTypeAi, recTypeLo} recType;

typedef struct devIp330Pvt {
    asynUser          *pasynUser;
    asynInt32Callback *int32Callback;
    void              *int32CallbackPvt;
    asynIp330         *ip330;
    void              *ip330Pvt;
    epicsMutexId      mutexId;
    int               channel;
    int               command;
    recType           recType;
    double            sum;
    int               numAverage;
} devIp330Pvt;

typedef struct dsetIp330 {
    long       number;
    DEVSUPFUN  report;
    DEVSUPFUN  init;
    DEVSUPFUN  init_record;
    DEVSUPFUN  get_ioint_info;
    DEVSUPFUN  io;
    DEVSUPFUN  convert;
} dsetIp330;

static long initCommon(dbCommon *pr, DBLINK *plink, recType rt, 
                       userCallback callback, char **up);

static long initAi(aiRecord *pr);
static long readAi(aiRecord *pr);
static void callbackAi(asynUser *pasynUser);
static void dataCallbackAi(void *drvPvt, epicsInt32 value);
static long convertAi(aiRecord *pai, int pass);
static long initLo(longoutRecord *pr);
static long writeLo(longoutRecord *pr);
static void callbackLo(asynUser *pasynUser);

dsetIp330 devAiIp330 = {6, 0, 0, initAi, 0, readAi, convertAi};
dsetIp330 devLoIp330 = {6, 0, 0, initLo, 0, writeLo, 0};

epicsExportAddress(dset, devAiIp330);
epicsExportAddress(dset, devLoIp330);


static long initCommon(dbCommon *pr, DBLINK *plink, recType rt, 
                       userCallback callback, char **up)
{
    int i;
    char port[100];
    struct vmeio *pio = (struct vmeio *)plink;
    devIp330Pvt *pPvt;
    asynUser *pasynUser=NULL;
    asynInterface *pasynInterface;
    asynStatus status;

    pPvt = callocMustSucceed(1, sizeof(devIp330Pvt), 
                             "devIp330::initCommon");
    pr->dpvt = pPvt;
    pPvt->recType = rt;

    /* Fetch the port field */
    if (plink->type != VME_IO) {
        errlogPrintf("devIp330::initCommon %s link is not VME link\n", 
                     pr->name);
        goto bad;
    }
    pPvt->channel = pio->signal;
    /* First field of parm is the port name */
    for (i=0; pio->parm[i] && pio->parm[i]!=',' && pio->parm[i]!=' '
                           && i<100; i++)
       port[i]=pio->parm[i];
    port[i]='\0';

    *up = &pio->parm[i+1];
    pasynUser = pasynManager->createAsynUser(callback, 0);
    pPvt->pasynUser = pasynUser;
    pasynUser->userPvt = pr;
    status = pasynManager->connectDevice(pasynUser, port, pio->signal);
    if (status != asynSuccess) {
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
                  "devIp330::initCommon, error in connectDevice %s\n",
                  pasynUser->errorMessage);
        goto bad;
    }
    pasynInterface = pasynManager->findInterface(pasynUser, asynIp330Type, 1);
    if (!pasynInterface) {
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
                  "devIp330::initCommon, cannot find ip330 interface %s\n",
                  pasynUser->errorMessage);
        goto bad;
    }
    pPvt->ip330 = (asynIp330 *)pasynInterface->pinterface;
    pPvt->ip330Pvt = pasynInterface->drvPvt;
    pasynInterface = pasynManager->findInterface(pasynUser, 
                                                 asynInt32CallbackType, 1);
    if (!pasynInterface) {
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
                  "devIp330::initCommon, cannot find int32Callback interface %s\n",
                  pasynUser->errorMessage);
        goto bad;
    }
    pPvt->int32Callback = (asynInt32Callback *)pasynInterface->pinterface;
    pPvt->int32CallbackPvt = pasynInterface->drvPvt;
    return(0);
bad:
    pr->pact = 1;
    return(-1);
}


static long initAi(aiRecord *pai)
{
    char *up;
    devIp330Pvt *pPvt;
    int status;
    int gain;

    status = initCommon((dbCommon *)pai, &pai->inp, recTypeAi, callbackAi, &up);
    if (status) return 0;

    pPvt = (devIp330Pvt *)pai->dpvt;
    if (pPvt->channel < 0 || pPvt->channel > 31) {
        errlogPrintf("devIp330::initAi Invalid signal #: %s = %dn", 
                     pai->name, pPvt->channel);
        pai->pact=1;
    }
    if(up && strlen(up)>0) {
        /* decode the user parm field, format: "gain" */
        if((sscanf(up,"%d",&gain)<1) || gain<0 || gain>3) {
            errlogPrintf("devIp330::initAi %s illegal gain\n", pai->name);
            pai->pact=1;
        }
    }
    pPvt->mutexId = epicsMutexCreate();
    pPvt->int32Callback->registerCallback(pPvt->int32CallbackPvt,
                                          pPvt->pasynUser,
                                          dataCallbackAi, pPvt);
    pPvt->ip330->setGain(pPvt->ip330Pvt, pPvt->pasynUser, gain);
    convertAi(pai, 1);
    return(0);
}

static void dataCallbackAi(void *drvPvt, epicsInt32 value)
{
    devIp330Pvt *pPvt = (devIp330Pvt *)drvPvt;

    epicsMutexLock(pPvt->mutexId);
    pPvt->numAverage++;
    pPvt->sum += value;
    epicsMutexUnlock(pPvt->mutexId);
}

static void callbackAi(asynUser *pasynUser)
{
    /* This should never be called */
    asynPrint(pasynUser, ASYN_TRACE_ERROR,
              "devIp330::callbackAi, should not be called\n");
}

static long readAi(aiRecord *pai)
{
    devIp330Pvt *pPvt = (devIp330Pvt *)pai->dpvt;
    int data;

    epicsMutexLock(pPvt->mutexId);
    if (pPvt->numAverage == 0) pPvt->numAverage = 1;
    data = pPvt->sum/pPvt->numAverage + 0.5;
    pPvt->numAverage = 0;
    pPvt->sum = 0.;
    epicsMutexUnlock(pPvt->mutexId);
    pai->rval = data;
    pai->udf=0;
    asynPrint(pPvt->pasynUser, ASYN_TRACEIO_DEVICE,
              "devIp330::readAi %s value=%d\n",
              pai->name, pai->rval);
    return(0);
}

static long convertAi(aiRecord *pai, int pass)
{
    pai->eslo=(pai->eguf-pai->egul)/(double)0xffff;
    return 0;
}


static long initLo(longoutRecord *plo)
{
    char *up;
    int status;

    status = initCommon((dbCommon *)plo, &plo->out, recTypeLo, callbackLo, &up);
    return(0);
}

static long writeLo(longoutRecord *plo)
{
    devIp330Pvt *pPvt = (devIp330Pvt *)plo->dpvt;

    pasynManager->queueRequest(pPvt->pasynUser, 0, 0);
    return(0);
}

static void callbackLo(asynUser *pasynUser)
{
    longoutRecord *plo = (longoutRecord *)pasynUser->userPvt;
    devIp330Pvt *pPvt = (devIp330Pvt *)plo->dpvt;

    asynPrint(pPvt->pasynUser, ASYN_TRACEIO_DEVICE,
              "devIp330::writeLo %s command=%d, channel=%d, value=%d\n",
              plo->name, pPvt->command, pPvt->channel, plo->val);
    pPvt->ip330->setSecondsBetweenCalibrate(pPvt->ip330Pvt, 
                                            pPvt->pasynUser, 
                                            plo->val);
}
