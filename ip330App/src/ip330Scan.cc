//ip330Scan.cc

/********************COPYRIGHT NOTIFICATION**********************************
This software was developed under a United States Government license
described on the COPYRIGHT_UniversityOfChicago file included as part
of this distribution.
****************************************************************************/
/*
    Original Author: Jim Kowalkowski
    Date: 2/15/95
    Current Authors: Mark Rivers, Joe Sullivan and Marty Kraimer
    Converted to MPF: 12/9/98
    10/27/99 MLR Converted to use ip330 (base class): 
    04/20/00 MLR Changed logic for timing, no longer support changing clock
                 rate of Ip330.
    01/16/01 MLR Added check for valid pIp330 in Ip330Scan::init
    03/31/03 MLR Made time to average be independent for each channel, previously
                 all channels used the same time. Added some debugging.
*/

#include <vxWorks.h>
#include <iv.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

#include "Ip330Scan.h"

extern "C"
{
#ifdef NODEBUG
#define DEBUG(l,f,v) ;
#else
#define DEBUG(l,f,v...) { if(l<Ip330ScanDebug) printf(f,## v); }
#endif
volatile int Ip330ScanDebug = 0;
}


Ip330Scan * Ip330Scan::init(Ip330 *pIp330, int firstChan, int lastChan,
                            int milliSecondsToAverage)
{
    if (!pIp330) return(0);
    Ip330Scan *pIp330Scan = new Ip330Scan(pIp330,firstChan,lastChan,milliSecondsToAverage);
    return(pIp330Scan);
}

Ip330Scan:: Ip330Scan(
    Ip330 *pIp330, int firstChan, int lastChan, int milliSecondsToAverage)
: pIp330(pIp330), firstChan(firstChan), lastChan(lastChan)
{
    int chan;
    chanData = (Ip330ScanData *)calloc(MAX_IP330_CHANNELS,sizeof(Ip330ScanData));
    if(!chanData) {
        printf("Ip330Scan calloc failed\n");
        return;
    }
    for (chan=0; chan<MAX_IP330_CHANNELS; chan++) {
       chanData[chan].milliSecondsToAverage = -1;
       setNumAverage(milliSecondsToAverage, chan);
    }
    pIp330->registerCallback(callback, (void *)this);
}

int Ip330Scan::getValue(int channel)
{
    DEBUG(5, "Ip330Scan:getValue, chan=%d, chanVal=%ld, accumulated=%d, averageStore=%ld\n",
                               channel, chanData[channel].chanVal, 
                               chanData[channel].accumulated, 
                               chanData[channel].averageStore);
    return( pIp330->correctValue(channel, chanData[channel].chanVal) );
}

void Ip330Scan:: callback(void *v, int *newData)
{
    Ip330Scan      *t = (Ip330Scan *) v;
    int             i;
    Ip330ScanData  *sd;

    for (i = t->firstChan; i <= t->lastChan; i++) {
        sd = &t->chanData[i];
        sd->accumulated++;
        sd->averageStore += newData[i];
        if (sd->accumulated == sd->numAverage) {
            sd->chanVal = sd->averageStore / sd->accumulated;
            sd->averageStore = 0;
            sd->accumulated = 0;
        }
    }
}

int Ip330Scan:: setGain(int gain, int channel)
{
    pIp330->setGain(gain, channel);
    return(0);
}


void Ip330Scan:: setNumAverage(int milliSecondsToAverage, int channel)
{
   // If the time is the same as currently set don't do anything
   if (milliSecondsToAverage == chanData[channel].milliSecondsToAverage) return;
   DEBUG(1, "Ip330Scan: changed time to average from %d to %d\n", 
            chanData[channel].milliSecondsToAverage, milliSecondsToAverage);
   chanData[channel].milliSecondsToAverage = milliSecondsToAverage;
   chanData[channel].numAverage = (int) (((1000. * milliSecondsToAverage) / 
                                 pIp330->getMicroSecondsPerScan()) + 0.5);
   chanData[channel].averageStore = 0;
   chanData[channel].accumulated = 0;
}

