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

*/

#ifndef ip330ScanH
#define ip330ScanH

#include "Ip330.h"

class Ip330ScanData;
	
class Ip330Scan
{
public:
    static Ip330Scan * init(Ip330 *pIp330, int firstChan, int lastChan, 
    	                    int milliSecondsToAverage);
    int getValue(int channel);
    int setGain(int gain,int channel);
    void setMilliSecondsToAverage(int milliSeconds);
private:
    Ip330Scan(Ip330 *pIp330, int firstChan, int lastChan, 
    	int milliSecondsToAverage);
    static void callback(void*, int *data);   // Callback function from Ip330
    void setNumAverage(int milliSecondsToAverage);
    Ip330 *pIp330;
    int firstChan;
    int lastChan;
    int numAverage;
    int accumulated;
    Ip330ScanData *chanData;
};

#endif //ip330ScanH
