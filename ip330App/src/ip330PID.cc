//ip330PID.cc

/********************COPYRIGHT NOTIFICATION**********************************
This software was developed under a United States Government license
described on the COPYRIGHT_UniversityOfChicago file included as part
of this distribution.
****************************************************************************/
/*
    Author: Mark Rivers
    10/27/99

    04/20/00  MLR  Changed timing logic.  No longer reprogram IP-330 clock,
                   just change whether the callback is executed when called.
    05/02/00  MLR  Fixed bug in time calculation for integral term
    05/03/00  MLR  Added more sanity checks to integral term:
                   If KI is 0. then set I to DRVL if KP is greater than 0,
                   set I to DRVH is KP is less than 0.
                   If feedback is off don't change integral term.
    05/17/00  MLR  Added another sanity checks to integral term:
                   When feedback goes from OFF to ON then set integral term
                   equal to the current value of the DAC output.  This ensures
                   a "bumpless" turn-on of feedback
    01/16/01 MLR Added check for valid pIp330 in Ip330PID::init
*/

#include <vxWorks.h>
#include <iv.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <tickLib.h>

#include "Ip330PID.h"

extern "C"
{
#ifdef NODEBUG
#define DEBUG(l,f,v) ;
#else
#define DEBUG(l,f,v...) { if(l<Ip330PIDDebug) printf(f,## v); }
#endif
volatile int Ip330PIDDebug = 0;
}


Ip330PID * Ip330PID::init(Ip330 *pIp330, int ADCChannel, 
                          DAC128V *pDAC128V, int DACChannel)

{
    if (!pIp330) return(0);
    Ip330PID *pIp330PID = new Ip330PID(pIp330, ADCChannel, 
                           pDAC128V, DACChannel);

    return(pIp330PID);
}

Ip330PID:: Ip330PID(
   Ip330 *pIp330, int ADCChannel, DAC128V *pDAC128V, int DACChannel)
: KP(1), KI(0), KD(0), 
  P(0), I(0), D(0), feedbackOn(0), 
  lowLimit(1), highLimit(-1), 
  pIp330(pIp330), ADCChannel(ADCChannel), 
  pDAC128V(pDAC128V), DACChannel(DACChannel), prevError(0)
{
    // Nothing else to do in constructor
}

void Ip330PID::config(Ip330PID *pIp330PID, double KKP, double KKI, double KKD, 
                      int microSeconds, int feedback,
                      int low, int high)
{
    pIp330PID->KP = KKP;
    pIp330PID->KI = KKI;
    pIp330PID->KD = KKD;
    pIp330PID->setMicroSecondsPerScan((float)microSeconds);
    pIp330PID->feedbackOn = feedback;
    pIp330PID->lowLimit = low;
    pIp330PID->highLimit = high;
    pIp330PID->pIp330->registerCallback(callback, (void *)pIp330PID);
}


void Ip330PID:: callback(void *v, int *newData)
{
    Ip330PID        *t = (Ip330PID *) v;
    t->doPID(newData);
}

void Ip330PID:: doPID(int *newData)
{
    double dt;
    double derror;
    double dI;
    int currentDAC;

    if (++skip <= numSkip) return;  // Only execute every "numSkip+1" calls
    skip = 0;
    dt = getMicroSecondsPerScan() / 1.e6;
    actual = newData[ADCChannel];
    prevError = error;
    error = setPoint - actual;
    derror = error - prevError;
    P = KP*error;
    /* Sanity checks on integral term:
        * 1) Don't increase I if output >= highLimit
        * 2) Don't decrease I if output <= lowLimit
        * 3) Don't change I if feedback is off
        * 4) Limit the integral term to be in the range betweem DRLV and DRVH
        * 5) If KI is zero then set the sum to DRVL (if KI is positive), or
        *    DRVH (if KP is negative) to allow easily turning off the 
        *    integral term for PID tuning.
    */
    dI = KP*KI*error*dt;
    if (feedbackOn) {
        if (!prevFeedbackOn) {
            pDAC128V->getValue(&currentDAC, DACChannel);
            I = currentDAC;
        }
        else {
            if (((output > lowLimit) && (output < highLimit)) ||
                ((output >= highLimit) && ( dI < 0.)) ||
                ((output <= lowLimit)  && ( dI > 0.))) {
                I = I + dI;
                if (I < lowLimit) I = lowLimit;
                if (I > highLimit) I = highLimit;
            }
        }
    }
    if (KI == 0.) {
        if (KP > 0.) I = lowLimit; else I = highLimit;
    }
    if (dt>0.0) D = KP*KD*(derror/dt); else D = 0.0;
    output = (int)(P + I + D);
    // Limit output to range from DRLV to DRVH
    if (output > highLimit) output = highLimit;
    if (output < lowLimit) output = lowLimit;

    // If feedback is on write output to DAC
    if (feedbackOn) {
        pDAC128V->setValue(output, DACChannel);
    }
    // Save state of feedback
    prevFeedbackOn = feedbackOn;
}

float Ip330PID:: setMicroSecondsPerScan(float microSeconds)
{
    numSkip = (int) (microSeconds /
                        pIp330->getMicroSecondsPerScan() + 0.5) - 1;
    if (numSkip < 0) numSkip = 0;
    skip = 0;
    return getMicroSecondsPerScan();
}   


float Ip330PID:: getMicroSecondsPerScan()
{
    return(pIp330->getMicroSecondsPerScan() * (numSkip+1));
}   
