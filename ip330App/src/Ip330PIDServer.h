//Ip330PIDServer.h

/********************COPYRIGHT NOTIFICATION**********************************
This software was developed under a United States Government license
described on the COPYRIGHT_UniversityOfChicago file included as part
of this distribution.
****************************************************************************/
/*
    Author: Mark Rivers
    Date: 10/27/99

*/

#ifndef ip330PIDServerH
#define ip330PIDServerH

enum ip330PIDCommands  {cmdStartFeedback, cmdStopFeedback, cmdSetHighLimit,
                        cmdSetLowLimit, cmdSetKP, cmdSetKI, cmdSetKD,
                        cmdSetI, cmdSetSecondsPerScan, cmdSetSetPoint, 
                        cmdGetParams};
#define numIp330PIDOffsets 7
enum ip330PIDOffsets {offsetActual, offsetError, offsetOutput, 
                      offsetP, offsetI, offsetD, offsetSecondsPerScan};
                        
#endif //ip330PIDH
