//Ip330.h

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
    27-Oct-1999  MLR  Converted to ip330 (base class) from ip330Scan: 
    31-Mar-2003  MLR  Added MAX_IP330_CHANNELS definition

*/

#ifndef ip330H
#define ip330H

#include <fppLib.h>
#include <semLib.h>

#define MAX_IP330_CHANNELS 32

class WatchDog;
class MessageServer;
class IndustryPackModule;
class ip330ADCregs;
class ip330ADCSettings;

enum signalType {differential, singleEnded};
enum scanModeType {disable, uniformContinuous, uniformSingle, burstContinuous,
	           burstSingle, convertOnExternalTriggerOnly};
enum triggerType {input, output};

	
typedef void (*Ip330Callback)(void *pvt, int *data);

class Ip330
{
public:
    static Ip330 * init(
        const char *moduleName, const char *carrierName, const char *siteName,
        const char *type, const char *range, int firstChan, int lastChan,
        int maxClients, int intVec);
    int config(scanModeType scanMode, const char *triggerString, int
    	microSecondsPerScan, int secondsBetweenCalibrate);
    int getCorrectedValue(int channel);
    int correctValue(int channel, int raw);
    int getRawValue(int channel);
    int setGain(int gain,int channel);
    int setScanMode(scanModeType scanMode);
    int setTrigger(triggerType trigger);
    float setMicroSecondsPerScan(float microSeconds);
    float getMicroSecondsPerScan();
    void setSecondsBetweenCalibrate(int seconds);
    int registerCallback(Ip330Callback callback, void *pvt);
private:
    Ip330(IndustryPackModule *pIndustryPackModule,
             signalType type, int range, int firstChan, int lastChan,
             int maxClients, int intVec);
    int setGain(int range, int gain, int channel);
    static void intFunc(void*); // Interrupt function
    static void rebootCallback(void *);
    float getActualMicroSecondsPerScan();
    float setTimeRegs(float microSecondsPerScan);
    int calibrate(int channel);
    void waitNewData();
    static void autoCalibrate(void *);
    IndustryPackModule *pIPM;
    WatchDog *wdId;
    SEM_ID lock;
    FP_CONTEXT *pFpContext;
    signalType type;
    int range;
    volatile ip330ADCregs* regs;
    ip330ADCSettings *chanSettings;
    int chanData[MAX_IP330_CHANNELS];
    int firstChan;
    int lastChan;
    scanModeType scanMode;
    triggerType trigger;
    int secondsBetweenCalibrate;
    int maxClients;
    bool rebooting;
    int numClients;
    int mailBoxOffset;
    float actualMicroSecondsPerScan;
    void **clientPvt;
    Ip330Callback *clientCallback;
};

#endif //ip330H
