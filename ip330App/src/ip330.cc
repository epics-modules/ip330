//ip330.cc

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
    Converted to ip330 (base class) from ip330Scan: MLR 10/27/99

    16-Mar-2000  Mark Rivers.  
                 Added code to only to fppSave and fppRestore if fpp hardware
                 is present.
    28-Oct-2000  Mark Rivers.  
                 Fixed logic in setTimeRegs() to only correct for
                 the 15 microseconds per channel if scanMode is
                 burstContinuous.
                 Logic does uniformContinuous sensibly now,
                 microSecondsPerScan is the time for a
                 complete loop to measure all channels.
    01-Nov-2000  Marty Kraimer.
                 After calibration set mailBoxOffset=16
                 Ignore first sample during calibration. ADC needs settle time.
                 If secondsBetween Calibrate<0 always return raw value
                 Use lock while using/modifying adj_slope and adj_offset
    01-Jan-2001  Ned Arnold, Marty Kraimer
                 Moved statement loading interrupt vector.  This fixes a bug which
                 crashed the IOC if an external trigger is active when the ioc is
                 booted via a power cycle.
    28-Aug-2001  Mark Rivers per Marty Kraimer
                 Removed calls to intClear and intDisable.
                 These were not necessary and did not work on dumb IP carrier.
    12-Oct-2001  Marty Kraimer. Code to handle soft reboots.
    31-Jul-2002  Carl Lionberger and Eric Snow
                 Moved interupt enable to end of config, properly masked mode 
                 control bits in setScanMode, and adjusted mailBoxOffset for 
                 uniformSingle and burstSingle scan modes in intFunc.
*/

#include <vxWorks.h>
#include <iv.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

#include <taskLib.h>
#include <tickLib.h>
#include <intLib.h>

#include "Reboot.h"
#include "Ip330.h"
#include "WatchDog.h"
#include "IndustryPackModule.h"


#define SCAN_DISABLE    0xC8FF  // control register AND mask -
                                // SCAN and INTERRUPT disable
#define ACROMAG_ID 0xa3
#define ACRO_IP330 0x11

// Define default configuration parameters
#define SCAN_MODE burstContinuous
#define TRIGGER_DIRECTION input
#define MICROSECONDS_PER_SCAN 1000
#define SECONDS_BETWEEN_CALIBRATE 0

extern "C"
{
#ifdef NODEBUG
#define DEBUG(l,f,v) ;
#else
#define DEBUG(l,f,v...) { if(l<Ip330Debug) printf(f,## v); }
#endif
int Ip330Debug = 0;
}

static const int nRanges=4;
static const int nGains=4;
static const int nTriggers=2;
static const char *rangeName[nRanges] = {"-5to5","-10to10","0to5","0to10"};
static const char *triggerName[nTriggers] = {"Input", "Output"};
static const double pgaGain[nGains] = {1.0,2.0,4.0,8.0};

class ip330ADCregs
{
public:
    unsigned short control;
    unsigned char timePrescale;
    unsigned char intVector;
    unsigned short conversionTime;
    unsigned char endChanVal;
    unsigned char startChanVal;
    unsigned short newData[2];
    unsigned short missedData[2];
    unsigned short startConvert;
    unsigned char pad[0x0E];
    unsigned char gain[32];
    unsigned short mailBox[32];
};

class ip330ADCSettings
{
public:
    double volt_callo;
    double volt_calhi;
    unsigned char ctl_callo;
    unsigned char ctl_calhi;
    double ideal_span;
    double ideal_zero;
    double adj_slope;
    double adj_offset;
    int gain;
};

class callibrationSetting{
public:
        double volt_callo;
        double volt_calhi;
        unsigned char ctl_callo;
        unsigned char ctl_calhi;
        double ideal_span;
        double ideal_zero;
};

static callibrationSetting callibrationSettings[nRanges][nGains] = {
    {   {0.0000, 4.9000,  0x38,  0x18, 10.0,  -5.0},
        {0.0000, 2.4500,  0x38,  0x20, 10.0,  -5.0},
        {0.0000, 1.2250,  0x38,  0x28, 10.0,  -5.0},
        {0.0000, 0.6125,  0x38,  0x30, 10.0,  -5.0} },
    {   {0.0000, 4.9000,  0x38,  0x18, 20.0, -10.0},
        {0.0000, 4.9000,  0x38,  0x18, 20.0, -10.0},
        {0.0000, 2.4500,  0x38,  0x20, 20.0, -10.0},
        {0.0000, 1.2250,  0x38,  0x28, 20.0, -10.0} },
    {   {0.6125, 4.9000,  0x30,  0x18,  5.0,   0.0},
        {0.6125, 2.4500,  0x30,  0x20,  5.0,   0.0},
        {0.6125, 1.2250,  0x30,  0x28,  5.0,   0.0},
        {0.0000, 0.6125,  0x38,  0x30,  5.0,   0.0} },
    {   {0.6125, 4.9000,  0x30,  0x18, 10.0,   0.0},
        {0.6125, 4.9000,  0x30,  0x18, 10.0,   0.0},
        {0.6125, 2.4500,  0x30,  0x20, 10.0,   0.0},
        {0.6125, 1.2250,  0x30,  0x28, 10.0,   0.0} }
};

extern "C" Ip330 *initIp330(
    const char *moduleName, const char *carrierName, const char *siteName,
    const char *typeString, const char *rangeString,
    int firstChan, int lastChan,
    int maxClients, int intVec)
{
    Ip330 *pIp330 = Ip330::init(moduleName, carrierName, siteName, typeString,
                                rangeString, firstChan, lastChan,
                                maxClients, intVec);
    return pIp330;
}

extern "C" int configIp330(
    Ip330 *pIp330, 
    scanModeType scanMode, const char *triggerString,
    int microSecondsPerScan, int secondsBetweenCalibrate)
{
    if (pIp330 == NULL) return(-1);
    pIp330->config(scanMode, triggerString, microSecondsPerScan,
                   secondsBetweenCalibrate);
    return(0);
}
    

Ip330 * Ip330::init(
    const char *moduleName, const char *carrierName, const char *siteName,
    const char *typeString, const char *rangeString,
    int firstChan, int lastChan,
    int maxClients, int intVec)
{
    IndustryPackModule *pIPM = IndustryPackModule::createIndustryPackModule(
        moduleName,carrierName,siteName);
    if(!pIPM) return(0);
    unsigned char manufacturer = pIPM->getManufacturer();
    unsigned char model = pIPM->getModel();
    if(manufacturer!=ACROMAG_ID) {
        printf("initIp330 manufacturer 0x%x not ACROMAG_ID\n",
            manufacturer);
        return(0);
    }
    if(model!=ACRO_IP330) {
       printf("initIp330 model 0x%x not a ACRO_IP330\n",model);
       return(0);
    }
    signalType type;
    if(::strcmp(typeString,"D")==0) {
        type = differential;
    } else if(::strcmp(typeString,"S")==0) {
        type = singleEnded;
    } else {
        printf("Illegal type. Must be \"D\" or \"S\"\n");
        return(0);
    }
    int range;
    for(range=0; range<nRanges; range++) {
        if(::strcmp(rangeString,rangeName[range])==0) break;
    }
    if(range>=nRanges) {
        printf("Illegal range\n");
        return(0);
    }
    Ip330 *pIp330 = new Ip330(pIPM,type,range,firstChan,lastChan,
                       maxClients, intVec);
    return(pIp330);
}

Ip330:: Ip330(
    IndustryPackModule *pIndustryPackModule,
    signalType type, int range, int firstChan, int lastChan,
    int maxClients, int intVec)
: pIPM(pIndustryPackModule), type(type), range(range),
  firstChan(firstChan), lastChan(lastChan),
  maxClients(maxClients), rebooting(0), numClients(0), mailBoxOffset(16)
{
    chanSettings = (ip330ADCSettings *)calloc(32,sizeof(ip330ADCSettings));
    if(!chanSettings) {
        printf("Ip330 calloc failed\n");
        return;
    }
    clientPvt = (void **)calloc(maxClients, sizeof(void *));
    if(!clientPvt) {
        printf("Ip330 calloc failed\n");
        return;
    }
    clientCallback = (Ip330Callback *)calloc(maxClients, sizeof(Ip330Callback*));
    if(!clientCallback) {
        printf("Ip330 calloc failed\n");
        return;
    }
    if (fppProbe()==OK) 
       pFpContext = (FP_CONTEXT *)calloc(1, sizeof(FP_CONTEXT));
    else
       pFpContext = NULL;
    regs = (ip330ADCregs *) pIPM->getMemBaseIO();
    wdId = new WatchDog;
    lock = semMCreate(SEM_Q_FIFO);
    regs->startConvert = 0x0000;
    pIPM->intConfig(0); pIPM->intConfig(1);
    regs->intVector = intVec;
    if(intConnect(INUM_TO_IVEC(intVec),(VOIDFUNCPTR)intFunc,(int)this)==ERROR){
        printf("Ip330 intConnect Failure\n");
    }
    Reboot::rebootHookAdd(rebootCallback,(void *)this);
    /* Program chip registers */
    regs->control = 0x0000;
    regs->control |= 0x0002; /* Output Data Format = Straight Binary */
    setTrigger(TRIGGER_DIRECTION);
    setScanMode(SCAN_MODE);
    regs->control |= 0x0800; /* Timer Enable = Enable */
    setTimeRegs(MICROSECONDS_PER_SCAN);
    if(type==differential) {
        regs->control |= 0x0000;
    } else {
        regs->control |= 0x0008;
    }
    /* Channels to convert */
    regs->startChanVal = firstChan;
    regs->endChanVal = lastChan;
    for (int i = firstChan; i <= lastChan; i++) {
        // default to gain of 0
        setGain(range,0,i);
    }
    setSecondsBetweenCalibrate(SECONDS_BETWEEN_CALIBRATE);
}

int Ip330:: config(scanModeType scan, const char *triggerString, 
               int microSeconds, int secondsCalibrate)
{
    if(rebooting) taskSuspend(0);
    int trigger;
    for(trigger=0; trigger<nTriggers; trigger++) {
        if(::strcmp(triggerString,triggerName[trigger])==0) break;
    }
    if(trigger>=nTriggers) {
        printf("Illegal trigger\n");
        return(-1);
    }
    setTrigger((triggerType)trigger);
    setScanMode(scan);
    setMicroSecondsPerScan(microSeconds);
    setSecondsBetweenCalibrate(secondsCalibrate);
    autoCalibrate((void *)this);
    regs->control |= 0x2000; /* = Interrupt After All Selected */
    return(0);
}

int Ip330::getCorrectedValue(int channel)
{
    if(rebooting) taskSuspend(0);
    int val;
    val = correctValue(channel, chanData[channel]);
    return(val);
}

int Ip330::correctValue(int channel, int raw)
{
    if(rebooting) taskSuspend(0);
    int value;
    if(secondsBetweenCalibrate<0) {
        value=raw;
    }else {
        semTake(lock,WAIT_FOREVER);
        value = (int) (chanSettings[channel].adj_slope *
        (((double)raw + chanSettings[channel].adj_offset)));
        semGive(lock);
    }
    if(Ip330Debug==3) {
        printf("channel %d raw %d corrected %d\n",
            channel,raw,value);
    }
    return(value);
}

int Ip330::getRawValue(int channel)
{
    if(rebooting) taskSuspend(0);
    return( chanData[channel]);
}

int Ip330::setGain(int gain,int channel)
{
    if(rebooting) taskSuspend(0);
    int status = 0;
    if(gain<0 || gain>nGains || channel<0 || channel>lastChan) return(-1);
    if(gain!=chanSettings[channel].gain) status = setGain(range,gain,channel);
    return(status);
}

int Ip330:: setGain(int range, int gain, int channel)
{
    if(rebooting) taskSuspend(0);
    unsigned short  saveControl;
    if(gain<0 || gain>=nGains) {
        printf("Ip330 setGain illegal gain value %d\n",gain);
        return(-1);
    }
    saveControl = regs->control;
    regs->control &= SCAN_DISABLE;
    chanSettings[channel].gain = gain;
    chanSettings[channel].volt_callo = callibrationSettings[range][gain].volt_callo;
    chanSettings[channel].volt_calhi = callibrationSettings[range][gain].volt_calhi;
    chanSettings[channel].ctl_callo = callibrationSettings[range][gain].ctl_callo;
    chanSettings[channel].ctl_calhi = callibrationSettings[range][gain].ctl_calhi;
    chanSettings[channel].ideal_span = callibrationSettings[range][gain].ideal_span;
    chanSettings[channel].ideal_zero = callibrationSettings[range][gain].ideal_zero;
    regs->gain[channel] = gain;
    regs->control = saveControl;
    calibrate(channel);
    return(0);
}

int Ip330::setTrigger(triggerType trig)
{
    if(rebooting) taskSuspend(0);
    if (trig < 0 || trig >= nTriggers) return(-1);
    trigger = trig;
    if (trigger == output) regs->control |= 0x0004;
    return(0);
}

int Ip330::setScanMode(scanModeType mode)
{
    if(rebooting) taskSuspend(0);
    if ((mode < disable) || (mode > convertOnExternalTriggerOnly)) return(-1);
    scanMode = mode;
    regs->control &= ~(0x7 << 8);		// Kill all control bits first
    regs->control |= scanMode << 8;
    return(0);
}


int Ip330::registerCallback(Ip330Callback callback, void *pvt)
{
    if(rebooting) taskSuspend(0);
    // Disable interrupts while adding this callback
    int intKey = intLock();
    DEBUG(1,"Ip330::registerCallack, callback=%p, pvt=%p\n",
       callback, pvt);
    if (numClients >= maxClients) {
       printf("Ip330::registerClient: too many clients\n");
       return(-1);
    }
    numClients = numClients + 1;
    clientPvt[numClients-1] = pvt;
    clientCallback[numClients-1] = callback;
    intUnlock(intKey);
    return(0);
}
       
void Ip330:: intFunc(void *v)
{
    Ip330 *t = (Ip330 *) v;
    int    i;

    // Save and restore FP registers so application interrupt functions can do
    // floating point operations.  Skip if no fpp hardware present.
    if (t->pFpContext != NULL) fppSave (t->pFpContext);
    if (t->type == differential) {
       // Must alternate between reading data from mailBox[i] and mailBox[i+16]
       // Except in case of uniform/burstSingle, where only half of mailbox is 
       // used.
       if( t->scanMode == uniformSingle || t->scanMode==burstSingle 
                                        || t->mailBoxOffset==16)
          t->mailBoxOffset = 0; 
       else 
          t->mailBoxOffset = 16;
    }
    for (i = t->firstChan; i <= t->lastChan; i++) {
        t->chanData[i] = (t->regs->mailBox[i + t->mailBoxOffset]);
    }
    // Call the callback routines which have registered
    for (i = 0; i < t->numClients; i++) {
       t->clientCallback[i]( t->clientPvt[i], t->chanData);
    }
    if (t->pFpContext != NULL) fppRestore(t->pFpContext);
    if(!t->rebooting) t->pIPM->intEnable(0);
}


#define MAX_TIMES 1000000
void Ip330:: waitNewData()
{
    int ntimes=0;
    while(ntimes++ < MAX_TIMES) {
        if(regs->newData[1]==0xffff) return;
    }
    printf("Ip330::waitNewData time out\n");
}

void Ip330::setSecondsBetweenCalibrate(int seconds)
{
    if(rebooting) taskSuspend(0);
    secondsBetweenCalibrate = seconds;
    autoCalibrate((void *)this);
}

void Ip330::autoCalibrate(void * parm)
{
    Ip330 *pIp330 = (Ip330 *)parm;
    if(pIp330->rebooting) taskSuspend(0);
    pIp330->wdId->cancel();
    if(pIp330->secondsBetweenCalibrate<0) return;
    if(Ip330Debug) printf("Ip330::autoCalibrate starting calibration\n");
    for(int i=pIp330->firstChan; i<= pIp330->lastChan; i++)
        pIp330->calibrate(i);
    if (pIp330->secondsBetweenCalibrate != 0)
       pIp330->wdId->start(pIp330->secondsBetweenCalibrate,autoCalibrate,parm);
}


//See Acromag User's Manual for details about callibration
int Ip330:: calibrate(int channel)
{
    if(rebooting) taskSuspend(0);
    unsigned short saveControl = regs->control;
    regs->control &= SCAN_DISABLE;
    /* Disable scan mode and interrupts */
    // determine count_callo
    unsigned char saveStartChanVal = regs->startChanVal;
    unsigned char saveEndChanVal = regs->endChanVal;
    regs->endChanVal = 0x1F;
    regs->startChanVal = 0x00;
    for (int i = 0; i < 32; i++) regs->gain[i] = chanSettings[channel].gain;
    regs->control = 0x0402 | (0x0038 & (chanSettings[channel].ctl_callo));
    regs->startConvert = 0x0001;
    waitNewData();
    // Ignore first set of data so that adc has time to settle
    regs->startConvert = 0x0001;
    waitNewData();
    if(Ip330Debug==2) printf("ip330 calibrate. Raw values low\n");
    long sum = 0;
    for (int i = 0; i < 32; i++) {
        unsigned short val = regs->mailBox[i];
        sum = sum + val;
        if(Ip330Debug==2) {
            printf("%hu ",val);
            if((i+1)%8 == 0)printf("\n");
       }
    }
    double count_callo = ((double)sum)/32.0;
    // determine count_calhi
    regs->control = 0x0402 | (0x0038 & (chanSettings[channel].ctl_calhi));
    regs->startConvert = 0x0001;
    waitNewData();
    // Ignore first set of data so that adc has time to settle
    regs->startConvert = 0x0001;
    waitNewData();
    if(Ip330Debug==2) printf("ip330 calibrate. Raw values high\n");
    sum = 0;
    for (int i = 0; i < 32; i++) {
        unsigned short val = regs->mailBox[i];
        sum = sum + val;
        if(Ip330Debug==2) {
            printf("%hu ",val);
            if((i+1)%8 == 0)printf("\n");
       }
    }
    double count_calhi = ((double)sum)/32.0;

    double m = pgaGain[chanSettings[channel].gain] *
        ((chanSettings[channel].volt_calhi - chanSettings[channel].volt_callo) /
         (count_calhi - count_callo));
    double cal1 = (65536.0 * m) / chanSettings[channel].ideal_span;
    double cal2 =
          ((chanSettings[channel].volt_callo * pgaGain[chanSettings[channel].gain])
            - chanSettings[channel].ideal_zero)
          / m - count_callo;
    semTake(lock,WAIT_FOREVER);
    chanSettings[channel].adj_slope = cal1;
    chanSettings[channel].adj_offset = cal2;
    semGive(lock);
    if(Ip330Debug) {
        printf("ip330 channel %d adj_slope %e adj_offset %e\n",
            channel,cal1,cal2);
    }
    //restore control and gain values
    regs->control &= SCAN_DISABLE;
    regs->control = saveControl;
    //Restore pre - calibrate control register state
    regs->startChanVal = saveStartChanVal;
    regs->endChanVal = saveEndChanVal;
    for (int i = 0; i < 32; i++) regs->gain[i] = chanSettings[i].gain;
    mailBoxOffset=16; /* make it start over*/
    if(!rebooting) pIPM->intEnable(0);
    regs->startConvert = 0x0001;
    return (0);
}

void Ip330:: rebootCallback(void *v)
{
    Ip330 *pIp330 = (Ip330 *)v;
    pIp330->regs->control &= SCAN_DISABLE;
    pIp330->rebooting = true;
}

float Ip330:: setMicroSecondsPerScan(float microSecondsPerScan)
{
    if(rebooting) taskSuspend(0);
    return(setTimeRegs(microSecondsPerScan));
}

float Ip330:: getActualMicroSecondsPerScan()
{
    if(rebooting) taskSuspend(0);
    return((15. * (lastChan - firstChan + 1)) +
          (regs->timePrescale * regs->conversionTime) / 8.);
}

// Note: we cache actualMicroSecondsPerScan for efficiency, so that
// getActualMicroSecondsPerScan is only called when the time registers are
// changed.  It is important for getMicroSecondsPerScan to be efficient, since
// it is often called from the interrupt routines of servers which use Ip330.
float Ip330:: getMicroSecondsPerScan()
{
    if(rebooting) taskSuspend(0);
    return(actualMicroSecondsPerScan);
}

float Ip330:: setTimeRegs(float microSecondsPerScan)
{
    if(rebooting) taskSuspend(0);
    // This function computes the optimal values for the prescale and
    // conversion timer registers.  microSecondsPerScan is the time
    // in microseconds between successive scans.
    // The delay time includes the 15 microseconds required to convert each 
    // channel.
    int timePrescale;
    int timeConvert;
    int status=0;
    float delayTime;
    if (scanMode == burstContinuous) 
       delayTime = microSecondsPerScan - 15. * (lastChan-firstChan+1);
    else if (scanMode == uniformContinuous) 
       delayTime = microSecondsPerScan / (lastChan - firstChan + 1);
    else
       delayTime = microSecondsPerScan;
    for (timePrescale=64; timePrescale<=255; timePrescale++) {
        timeConvert = (int) ((8. * delayTime)/(float)timePrescale + 0.5);
        if (timeConvert < 1) {
                printf("Ip330::setTimeRegs, time interval too short\n");
                timeConvert = 1;
                status=-1;
                goto finish;
        }
        if (timeConvert <= 65535) goto finish;
    }
    printf("Ip330::setTimeRegs, time interval too long\n");
    timeConvert = 65535;
    timePrescale = 255;
    status=-1;
finish:
    regs->timePrescale = timePrescale;
    regs->conversionTime = timeConvert;
    actualMicroSecondsPerScan = getActualMicroSecondsPerScan();
    DEBUG(1,"Ip330::setTimeRegs, requested time=%f\n", microSecondsPerScan);
    DEBUG(1,"Ip330::setTimeRegs, prescale=%d, convert=%d, actual=%f\n",
       timePrescale, timeConvert, actualMicroSecondsPerScan);
    if (status == 0) return(actualMicroSecondsPerScan); else return(-1.);
}
