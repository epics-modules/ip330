/* devIp330.c

    Author: Mark Rivers
    Created:  April 21, 2003, based on devAiIp330Scan.cc
    July 7, 2004 MLR Converted from MPF and C++ to asyn and C

    Device support for Ip330. Records supported include:
    longout - sends calibration interval in integer seconds

    NOTE: Gain is no longer supported, this must be added via an ao record 
*/

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <dbAccess.h>
#include <dbDefs.h>
#include <link.h>
#include <dbCommon.h>
#include <longoutRecord.h>
#include <recSup.h>
#include <devSup.h>
#include <dbScan.h>
#include <cantProceed.h>
#include <epicsPrint.h>
#include <epicsExport.h>
#include <epicsMutex.h>
#include <asynDriver.h>
#include <asynEpicsUtils.h>
#include <asynInt32Callback.h>

#include <recGbl.h>
#include <alarm.h>

#include "asynIp330.h"


typedef enum {recTypeAi, recTypeLo} recType;

typedef struct devIp330Pvt {
    asynUser          *pasynUser;
    asynIp330         *ip330;
    void              *ip330Pvt;
    int               channel;
    int               command;
    recType           recType;
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

static long initLo(longoutRecord *pr);
static long writeLo(longoutRecord *pr);
static void callbackLo(asynUser *pasynUser);

dsetIp330 devLoIp330 = {6, 0, 0, initLo, 0, writeLo, 0};

epicsExportAddress(dset, devLoIp330);


static long initCommon(dbCommon *pr, DBLINK *plink, recType rt, 
                       userCallback callback, char **up)
{
    char *port;
    devIp330Pvt *pPvt;
    asynUser *pasynUser;
    asynInterface *pasynInterface;
    asynStatus status;

    pPvt = callocMustSucceed(1, sizeof(devIp330Pvt), "devIp330::initCommon");
    pr->dpvt = pPvt;
    pPvt->recType = rt;

    pasynUser = pasynManager->createAsynUser(callback, 0);
    pPvt->pasynUser = pasynUser;
    pasynUser->userPvt = pr;
    status = pasynEpicsUtils->parseLink(pasynUser, plink, 
                                        &port, &pPvt->channel, up);
    if (status != asynSuccess) {
        errlogPrintf("devIp330::initCommon, error in VME link %s\n", 
                      pasynUser->errorMessage);
        goto bad;
    }
    status = pasynManager->connectDevice(pasynUser, port, pPvt->channel);
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
    return(0);
bad:
    pr->pact = 1;
    return(-1);
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
