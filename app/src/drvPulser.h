//======================================================//
// Name: drvPulser.h
// Purpose: Device support for Pulser
//
// Authors: J. Vamosi.
// Date Created: Dec 27, 2023

//======================================================//
#ifndef drvPulser_H
#define drvPulser_H

#include <epicsThread.h>
#include <epicsEvent.h>
#include <asynPortDriver.h>

#include "stmMsg.h"
#include "stmConv.h"

//User defines
#define PORT_PREFIX         "PORT_"
#define DEVICE_RW_TIMEOUT   0.25
#define MAX_CHANNELS        5
#define REQUEST_SIZE        1024
#define RESPONSE_SIZE       512

//Poller thread
#define DEFAULT_POLL_TIME   0.25

/* These are the strings that device support passes to drivers via
 * the asynDrvUser interface.
 * Drivers must return a value in pasynUser->reason that is unique
 * for that command.
 */
//Communication
#define PULSER_SET_BP_STRING             "SET_BP"
#define PULSER_CLEAR_BP_STRING           "CLEAR_BP"
#define PULSER_ENABLE_SINGLE_STRING      "ENABLE_SINGLE"
#define PULSER_DISABLE_SINGLE_STRING     "DISABLE_SINGLE"
#define PULSER_ENABLE_DISABLE_ALL_STRING "ENABLE_DISABLE_ALL"
#define PULSER_TRIGGER_READY_STRING      "TRIGGER_READY"
#define PULSER_GET_STATUS_STRING         "GET_STATUS"
#define PULSER_STATUS_STRING             "STATUS"
#define PULSER_SEND_BUILD_STRING         "SEND_BUILD"
#define PULSER_EXT_TRIG_STRING           "EXT_TRIG"
#define PULSER_SET_INT_DELAY_STRING      "SET_INT_DELAY"
#define PULSER_UPDATE_LIMITS_STRING      "UPDATE_LIMITS"
#define PULSER_SAFE_TRANSITION_STRING    "SAFE_TRANSITION"
#define PULSER_PURGE_CONFIG_STRING       "PURGE_CONFIG"
#define PULSER_SET_PULSER_CAL_STRING     "SET_PULSER_CAL"
#define PULSER_SET_DATE_TIME_STRING      "SET_DATE_TIME"
#define PULSER_GET_DATE_TIME_STRING      "GET_DATE_TIME"
#define PULSER_DATE_TIME_STRING          "DATE_TIME"
#define PULSER_INIT_STRING               "INIT"

#define PULSER_SET_BP2_STRING            "SET_BP2"

class drvPulser : public asynPortDriver {
public:
	drvPulser(const char *portName, const char* hostInfo);
	
	/* Make  sure to free everything */
	~drvPulser();

    /* These are the methods that we override from asynPortDriver */

    /* These functions are in the asynCommon interface */
    virtual asynStatus connect(asynUser *pasynUser);
    //virtual asynStatus getAddress(asynUser *pasynUser, int *address);

    /* These functions are in the asynUInt32Digital interface */
    virtual asynStatus writeUInt32Digital(asynUser *pasynUser, epicsUInt32 value, epicsUInt32 mask);
    virtual asynStatus readUInt32Digital(asynUser *pasynUser, epicsUInt32 *value, epicsUInt32 mask);

    /* These functions are in the asynInt32 interface */
    virtual asynStatus writeInt32(asynUser *pasynUser, epicsInt32 value);
    virtual asynStatus readInt32(asynUser *pasynUser, epicsInt32 *value);

    /* These functions are in the asynInt64 interface */
    //virtual asynStatus writeInt64(asynUser *pasynUser, epicsInt64 value);
    //virtual asynStatus readInt64(asynUser *pasynUser, epicsInt64 *value);

    /* These functions are in the asynFloat64 interface */
    virtual asynStatus writeFloat64(asynUser *pasynUser, epicsFloat64 value);
    virtual asynStatus readFloat64(asynUser *pasynUser, epicsFloat64 *value);

    /* These functions are in the asynFloat32Array interface */
    virtual asynStatus readFloat32Array(asynUser *pasynUser, epicsFloat32 *data, size_t maxChans, size_t *nactual);
    virtual asynStatus writeFloat32Array(asynUser *pasynUser, epicsFloat32 *data, size_t maxChans);

    /* These functions are in the asynOctet interface */
    virtual asynStatus writeOctet(asynUser *pasynUser, const char *value, size_t maxChars, size_t *nActual);
    virtual asynStatus readOctet(asynUser *pasynUser, char *value, size_t maxChars, size_t *nActual, int *eomReason);

    virtual asynStatus writeInt32Array(asynUser *pasynUser, epicsInt32 *value, size_t nElements);
    virtual asynStatus readInt32Array(asynUser *pasynUser, epicsInt32 *value, size_t nElements, size_t *nIn);
    
    virtual asynStatus readInt16Array(asynUser *pasynUser, epicsInt16 *value, size_t nElements, size_t *nIn);
    virtual asynStatus writeInt16Array(asynUser *pasynUser, epicsInt16 *value, size_t nElements);

    /* These are the methods that are new to this class */
    void pollerThread();
    bool pulserExiting_;

protected:
    /* Values used for pasynUser->reason, and indexes into the parameter library. */
    int setBp_;
    int clearBp_;
    int enableSingle_;
    int disableSingle_;
    int enableDisableAll_;
    int triggerReady_;
    int getStatus_;
    int sendBuild_;
    int extTrig_;
    int status_;
    int setIntDelay_;
    int updateLimits_;
    int safeTransition_;
    int purgeConfig_;
    int setBp2_;
    int setPulserCal_;
    int setDateTime_;
    int getDateTime_;
    int dateTime_;
    int init_;

private:
    /* Our data */
    bool initialized_;           /* If initialized successfully */
    bool isConnected_;           /* Connection status */
    char *portName_;             /* asyn port name for the user driver */
    char *octetPortName_;        /* asyn port name for the asyn octet port */
    char *hostInfo_;             /* host info (IP address,connection type, port)*/
    asynUser  *pasynUserOctet_;  /* asynUser for asynOctet interface to asyn octet port */
    asynUser  *pasynUserCommon_; /* asynUser for asynCommon interface to asyn octet port */
    asynUser  *pasynUserTrace_;  /* asynUser for asynTrace on this port */
    asynStatus ioStatus_;
    asynStatus prevIOStatus_;
    double pollTime_;
    bool forceCallback_;
    epicsThreadId pollerThreadId_;
    epicsEventId pollerEventId_;
    
    StmMsg *pStm;
    char* msgBuffer;
    
    int connectReadMetaData(char* octetPortName_);
    int sendReceive(char* commandStr, int value);
};

#endif /* drvPulser_H */