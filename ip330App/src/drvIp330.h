/* drvIp330.h

********************COPYRIGHT NOTIFICATION**********************************
This software was developed under a United States Government license
described on the COPYRIGHT_UniversityOfChicago file included as part
of this distribution.
****************************************************************************

    Original Author: Jim Kowalkowski
    Date: 2/15/95
    Current Authors: Mark Rivers, Joe Sullivan, and Marty Kraimer
    Converted to MPF: 12/9/98
    27-Oct-1999  MLR  Converted to ip330 (base class) from ip330Scan: 
    31-Mar-2003  MLR  Added MAX_IP330_CHANNELS definition
    11-Jul-2004  MLR  Converted from MPF to asyn and from C++ to C

    drvIp330.c supports the following standard interfaces:
    asynInt32          - reads a single value from a channel
    asynFloat64        - reads a single value from a channel
    asynInt32Callback  - calls a function with a new value for a channel
                         whenever a new value is read
    asynFloat64Callback - calls a function with a new value for a channel
                          whenever a new value is read

    It also supports the device-specific interface, asynIp330, defined below 

*/

#ifndef drvIp330H
#define drvIp330H

#include <asynDriver.h>

#define asynIp330Type "asynIp330"

typedef struct {
    asynStatus (*setGain)                   (void *drvPvt, asynUser *pasynUser, 
                                             int gain);
    asynStatus (*setSecondsBetweenCalibrate)(void *drvPvt, asynUser *pasynUser,
                                             double seconds);
} asynIp330;

#endif /* ip330H */
