//Ip330PID.h

/********************COPYRIGHT NOTIFICATION**********************************
This software was developed under a United States Government license
described on the COPYRIGHT_UniversityOfChicago file included as part
of this distribution.
****************************************************************************/
/*
    Author: Mark Rivers
    Date: 10/27/99

    04/20/00  MLR  Added new private variables for timing

*/

#ifndef ip330PIDH
#define ip330PIDH

#include "Ip330.h"
#include "DAC128V.h"

class Ip330PID
{
public:
    static Ip330PID * init(Ip330 *pIp330, int ADCChannel, 
                           DAC128V *pDAC128V, int DACChannel);
    static void config(Ip330PID *pIp330PID, double KP, double KI, double KD, 
                       int interval, int feedbackOn,
                       int lowLimit, int highLimit);
    float setMicroSecondsPerScan(float microSeconds);
    float getMicroSecondsPerScan();
    int setPoint;
    int actual;
    double error;
    double KP;
    double KI;
    double KD;
    double P;
    double I;
    double D;
    int feedbackOn;
    int prevFeedbackOn;
    int lowLimit;
    int highLimit;
    int output;
private:
    Ip330PID(Ip330 *pIp330, int ADCChannel, DAC128V *pDAC128V, int DACChannel);
    static void callback(void*, int *data);   // Callback function from Ip330
    void doPID(int *data);
    Ip330 *pIp330;
    int ADCChannel;
    DAC128V *pDAC128V;
    int DACChannel;
    double prevError;
    int numSkip;
    int skip;
};

#endif //ip330PIDH
