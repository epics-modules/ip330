//Ip330Scan.h

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
    10/27/99  MLR   Converted to use ip330 (base class)
    04/20/00  MLR   Changed private, no longer support changing rate of
                    ip330.
    03/31/03  MLR   Made time to average be independent for each channel, previously
                    all channels used the same time.  
                    Added fields to Ip330ScanData, and moved the definition of that class
                    into this file from ip330Scan.cc.
  *

*/

#ifndef ip330ScanH
#define ip330ScanH

#include "Ip330.h"

class Ip330ScanData
{
public:
    long chanVal;
    long averageStore;
    int numAverage;
    int milliSecondsToAverage;
    int accumulated;
};

	
class Ip330Scan
{
public:
    static Ip330Scan * init(Ip330 *pIp330, int firstChan, int lastChan, 
    	                    int milliSecondsToAverage);
    int getValue(int channel);
    int setGain(int gain,int channel);
    void setNumAverage(int milliSecondsToAverage, int channel);
private:
    Ip330Scan(Ip330 *pIp330, int firstChan, int lastChan, 
    	int milliSecondsToAverage);
    static void callback(void*, int *data);   // Callback function from Ip330
    Ip330 *pIp330;
    int firstChan;
    int lastChan;
    Ip330ScanData *chanData;
};

#endif //ip330ScanH
