//======================================================//
// Name: drvPulser.cpp
// Purpose: Device support for Pulser
//
// Authors: J. Vamosi
// Date Created: Dec 27, 2023

//======================================================//

/* ANSI C includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* EPICS includes */
#include <dbAccess.h>
#include <epicsStdio.h>
#include <epicsString.h>
#include <epicsThread.h>
#include <epicsMutex.h>
#include <epicsEvent.h>
#include <epicsExport.h>
#include <epicsPrint.h>
#include <epicsExit.h>
#include <cantProceed.h>
#include <errlog.h>
#include <iocsh.h>

/* Asyn includes */
#include <drvAsynIPPort.h>
#include <asynPortDriver.h>
#include <asynOctetSyncIO.h>
#include <asynCommonSyncIO.h>

#include "drvPulser.h"

static const char *driverName = "PULSER";
static void pollerThreadC(void *drvPvt);

//==========================================================//
// class drvPulser
//      Holds useful vars for interacting with Pulser hardware
//==========================================================//
drvPulser::drvPulser(const char *portName, const char* hostInfo)

   : asynPortDriver(portName,
                    MAX_CHANNELS, /* maxAddr */
                    asynInt32Mask | asynInt16ArrayMask | asynInt32ArrayMask | asynUInt32DigitalMask | asynFloat64Mask | asynFloat32ArrayMask | asynOctetMask | asynDrvUserMask, /* Interface mask */
                    asynInt32Mask | asynInt16ArrayMask | asynInt32ArrayMask | asynUInt32DigitalMask | asynFloat64Mask | asynFloat32ArrayMask | asynOctetMask,                   /* Interrupt mask */
                    ASYN_CANBLOCK | ASYN_MULTIDEVICE, /* asynFlags */
                    1, /* Autoconnect */
                    0, /* Default priority */
                    0), /* Default stack size*/

    pulserExiting_(false),
    initialized_(false),
    isConnected_(false),
    portName_(epicsStrDup(portName)),
    octetPortName_(NULL),
    hostInfo_(epicsStrDup(hostInfo)),
    ioStatus_(asynSuccess),
    prevIOStatus_(asynSuccess),
    pollTime_(DEFAULT_POLL_TIME),
    forceCallback_(true)
{
    int ipConfigureStatus;
    static const char *functionName = "drvPulser";

    pStm = new StmMsg;
    msgBuffer = new char[REQUEST_SIZE];
    
    //Communication parameters
    createParam(PULSER_SET_BP_STRING,             asynParamInt16Array,     &setBp_);
    createParam(PULSER_CLEAR_BP_STRING,           asynParamInt32,          &clearBp_);
    createParam(PULSER_ENABLE_SINGLE_STRING,      asynParamInt32,          &enableSingle_);
    createParam(PULSER_DISABLE_SINGLE_STRING,     asynParamInt32,          &disableSingle_);
    createParam(PULSER_ENABLE_DISABLE_ALL_STRING, asynParamInt32,          &enableDisableAll_);
    createParam(PULSER_TRIGGER_READY_STRING,      asynParamInt32,          &triggerReady_);
    createParam(PULSER_SEND_BUILD_STRING,         asynParamInt32,          &sendBuild_);
    createParam(PULSER_EXT_TRIG_STRING,           asynParamInt32,          &extTrig_);
    createParam(PULSER_GET_STATUS_STRING,         asynParamInt32,          &getStatus_);
    createParam(PULSER_STATUS_STRING,             asynParamFloat32Array,   &status_);
    createParam(PULSER_SET_INT_DELAY_STRING,      asynParamInt16Array,     &setIntDelay_);
    createParam(PULSER_UPDATE_LIMITS_STRING,      asynParamFloat32Array,   &updateLimits_);
    createParam(PULSER_SAFE_TRANSITION_STRING,    asynParamInt32,          &safeTransition_);
    createParam(PULSER_PURGE_CONFIG_STRING,       asynParamInt32,          &purgeConfig_);
    createParam(PULSER_SET_PULSER_CAL_STRING,     asynParamFloat32Array,   &setPulserCal_);
    createParam(PULSER_SET_DATE_TIME_STRING,      asynParamOctet,          &setDateTime_);
    createParam(PULSER_GET_DATE_TIME_STRING,      asynParamInt32,          &getDateTime_);
    createParam(PULSER_DATE_TIME_STRING,          asynParamOctet,          &dateTime_);
    createParam(PULSER_INIT_STRING,               asynParamInt32,          &init_);

    createParam(PULSER_SET_BP2_STRING,            asynParamInt16Array,     &setBp2_);

    /* Create octet port name */
    size_t prefixlen = strlen(PORT_PREFIX);
    size_t len = strlen(portName_) + strlen(PORT_PREFIX) + 1;
    octetPortName_ = (char*)malloc(len);
    memcpy(octetPortName_, PORT_PREFIX, prefixlen);
    memcpy(octetPortName_ + prefixlen, portName_, strlen(portName_) + 1);
    octetPortName_[len - 1] = '\0';
    
    //drvAsynIPPortConfigure("portName","hostInfo",priority,noAutoConnect,noProcessEos)
    ipConfigureStatus = drvAsynIPPortConfigure(octetPortName_, hostInfo_, 0, 0, 0);

    if (ipConfigureStatus) {
        asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
            "%s::%s, Unable to configure drvAsynIPPort %s",
            driverName, functionName, octetPortName_);
        return;
    }
 
    connectReadMetaData(octetPortName_);

    // update Status and DateTime
    //sendReceive((char*)"Status", 1);
    //sendReceive((char*)"DateTime", 1);
    
    /* Create the epicsEvent to wake up the pollerThread.*/
//    pollerEventId_ = epicsEventCreate(epicsEventEmpty);

    /* Create the thread to read registers if this is a read function code */
//    pollerThreadId_ = epicsThreadCreate("PulserPoller",
//            epicsThreadPriorityMedium,
//            epicsThreadGetStackSize(epicsThreadStackMedium),
//            (EPICSTHREADFUNC)pollerThreadC,
//            this);

    //epicsAtExit(pulserExitCallback, this);

    initialized_ = true;
}

drvPulser::~drvPulser() {
    if (hostInfo_)
        free(hostInfo_);
    if (portName_)
        free(portName_);
    if (octetPortName_)
        free(octetPortName_);
    
    pasynManager->disconnect(pasynUserOctet_);
    pasynManager->freeAsynUser(pasynUserOctet_);
    pasynUserOctet_ = NULL;

    delete pStm;
    delete msgBuffer;
}

/***********************/
/* asynCommon routines */
/***********************/
/* Connect */
asynStatus drvPulser::connect(asynUser *pasynUser)
{
    if (initialized_ == false) return asynDisabled;

    pasynManager->exceptionConnect(pasynUser);
    return asynSuccess;
}

/*
**  asynUInt32Digital support
*/
asynStatus drvPulser::readUInt32Digital(asynUser *pasynUser, epicsUInt32 *value, epicsUInt32 mask)
{
    int function = pasynUser->reason;
    static const char *functionName = "readUInt32Digital";
    
    *value = 0;

    printf("%s called with function: %d\n", functionName, function);
    
    return asynSuccess;
}

asynStatus drvPulser::writeUInt32Digital(asynUser *pasynUser, epicsUInt32 value, epicsUInt32 mask)
{
    int function = pasynUser->reason;
    static const char *functionName = "writeUInt32Digital";
    
    printf("%s called with function: %d, value:%d\n", functionName, function, value);

    return asynSuccess;
}

/*
**  asynInt32 support
*/
asynStatus drvPulser::readInt32 (asynUser *pasynUser, epicsInt32 *value)
{
    int function = pasynUser->reason;
    static const char *functionName = "readInt32";

    if (function == getStatus_ || function == getDateTime_)
        *value = 1;
    else
        *value = 0;

    printf("%s called with function: %d\n", functionName, function);
    
    return asynSuccess;
}

asynStatus drvPulser::writeInt32(asynUser *pasynUser, epicsInt32 value)
{
    int function = pasynUser->reason;
    static const char *functionName = "writeInt32";
    int msgLen;
    size_t nWrite;
    
    printf("%s called with function: %d, value:%d\n", functionName, function, value);

    if (function == clearBp_) {
        if (pStm->encodeU16Integer("ClearBP", value, msgBuffer, &msgLen) != 0) {
            if (pasynOctetSyncIO->write(pasynUserOctet_, msgBuffer, msgLen, DEVICE_RW_TIMEOUT, &nWrite) != asynSuccess)
                asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "Sending ClearBP failed\n");
        } else {
            asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "No meta index found for ClearBP\n");
        }    
    } else if (function == enableSingle_) {
        if (pStm->encodeU16Integer("EnableSingle", value, msgBuffer, &msgLen) != 0) {
            if (pasynOctetSyncIO->write(pasynUserOctet_, msgBuffer, msgLen, DEVICE_RW_TIMEOUT, &nWrite) != asynSuccess)
                asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "Sending EnableSingle failed\n");
        } else {
            asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "No meta index found for EnableSingle\n");
        }    
    } else if (function == disableSingle_) {
        if (pStm->encodeU16Integer("DisableSingle", value, msgBuffer, &msgLen) != 0) {
            if (pasynOctetSyncIO->write(pasynUserOctet_, msgBuffer, msgLen, DEVICE_RW_TIMEOUT, &nWrite) != asynSuccess)
                asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "Sending DisableSingle failed\n");
        } else {
            asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "No meta index found for DisableSingle\n");
        }
    } else if (function == enableDisableAll_) {
        if (pStm->encodeBoolean("EnableDisableAll", (char)value, msgBuffer, &msgLen) != 0) {
            if (pasynOctetSyncIO->write(pasynUserOctet_, msgBuffer, msgLen, DEVICE_RW_TIMEOUT, &nWrite) != asynSuccess)
                asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "Sending EnableDisableAll failed\n");
        } else {
            asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "No meta index found for EnableDisableAll\n");
        }    
    } else if (function == triggerReady_) {
        if (pStm->encodeBoolean("TriggerReady", (char)value, msgBuffer, &msgLen) != 0) {
            if (pasynOctetSyncIO->write(pasynUserOctet_, msgBuffer, msgLen, DEVICE_RW_TIMEOUT, &nWrite) != asynSuccess)
                asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "Sending TriggerReady failed\n");
        } else {
            asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "No meta index found for TriggerReady\n");
        }    
    } else if (function == sendBuild_) {
        if (pStm->encodeU16Integer("SendBuild", value, msgBuffer, &msgLen) != 0) {
            if (pasynOctetSyncIO->write(pasynUserOctet_, msgBuffer, msgLen, DEVICE_RW_TIMEOUT, &nWrite) != asynSuccess)
                asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "Sending SendBuild failed\n");
        } else {
            asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "No meta index found for SendBuild\n");
        }    
    } else if (function == extTrig_) {
        if (pStm->encodeBoolean("ExtTrig", (char)value, msgBuffer, &msgLen) != 0) {
            if (pasynOctetSyncIO->write(pasynUserOctet_, msgBuffer, msgLen, DEVICE_RW_TIMEOUT, &nWrite) != asynSuccess)
                asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "Sending ExtTrig failed\n");
        } else {
            asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "No meta index found for extTrig\n");
        }    
    } else if (function == safeTransition_) {
        if (pStm->encodeBoolean("SafeTransition", (char)value, msgBuffer, &msgLen) != 0) {
            if (pasynOctetSyncIO->write(pasynUserOctet_, msgBuffer, msgLen, DEVICE_RW_TIMEOUT, &nWrite) != asynSuccess)
                asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "Sending SafeTransition failed\n");
        } else {
            asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "No meta index found for SafeTransition");
        }    
    } else if (function == purgeConfig_) {
        if (pStm->encodeBoolean("PurgeConfig", (char)value, msgBuffer, &msgLen) != 0) {
            if (pasynOctetSyncIO->write(pasynUserOctet_, msgBuffer, msgLen, DEVICE_RW_TIMEOUT, &nWrite) != asynSuccess)
                asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "Sending PurgeConfig failed\n");
        } else {
            asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "No meta index found for PurgeConfig");
        }    
    } else if (function == init_) {
        if (pStm->encodeBoolean("Init", (char)value, msgBuffer, &msgLen) != 0) {
            if (pasynOctetSyncIO->write(pasynUserOctet_, msgBuffer, msgLen, DEVICE_RW_TIMEOUT, &nWrite) != asynSuccess)
                asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "Sending Init failed\n");
        } else {
            asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "No meta index found for Init");
        }    
    } else if (function == getStatus_) {
        sendReceive((char*)"Status", value);
    } else if (function == getDateTime_) {
        sendReceive((char*)"DateTime", value);
    } else {
        asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "%s::%s port %s invalid pasynUser->reason: %d\n",
            driverName, functionName, this->portName, function);
        return asynError;
    }
    
    return asynSuccess;
}
///////////////////////////////////////////////////////////////////////////////////

/*
**  asynInt32Array support
*/
asynStatus drvPulser::readInt32Array (asynUser *pasynUser, epicsInt32 *value, size_t nElements, size_t *nIn)
{
    int function = pasynUser->reason;
    static const char *functionName = "readInt32Array";

    printf("%s called with function:%d, value[0]:%d, elements:%ld\n", functionName, function, value[0], nElements);

    return asynSuccess;
}

asynStatus drvPulser::writeInt32Array(asynUser *pasynUser, epicsInt32 *value, size_t nElements)
{
    int function = pasynUser->reason;
    static const char *functionName = "writeInt32Array";
    
    printf("%s called with function: %d, value[0]:%d, elements:%ld\n", functionName, function, value[0], nElements);

    return asynSuccess;
}
///////////////////////////////////////////////////////////////////////////////////

/*
**  asynInt16Array support
*/
asynStatus drvPulser::readInt16Array(asynUser *pasynUser, epicsInt16 *value, size_t nElements, size_t *nIn)
{
    int function = pasynUser->reason;
    static const char *functionName = "readInt16Array";

    printf("%s called with function:%d, value[0]:%d, elements:%ld\n", functionName, function, value[0], nElements);

    return asynSuccess;
}
///////////////////////////////////////////////////////////////////////////////////

asynStatus drvPulser::writeInt16Array(asynUser *pasynUser, epicsInt16 *value, size_t nElements)
{
    int function = pasynUser->reason;
    static const char *functionName = "writeInt16Array";
    int msgLen;
    size_t nWrite;
    
    printf("%s called with function: %d, value[0]:%d, elements:%ld\n", functionName, function, value[0], nElements);

    for (unsigned int i=0; i<nElements; i++) {
        printf("value[%d]:%d\n", i, value[i]);
    }

    if (function == setBp_) {
        if (pStm->encodeU16IntegerArray("SetBP", value, nElements, msgBuffer, &msgLen) != 0) {
            if (pasynOctetSyncIO->write(pasynUserOctet_, msgBuffer, msgLen, DEVICE_RW_TIMEOUT, &nWrite) != asynSuccess)
                asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "Sending SetBP failed\n");
        } else {
            asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "No meta index found for SetBP\n");
        }    
    } else if (function == setIntDelay_) {
        if (pStm->encodeU16IntegerArray("SetIntDelay", value, nElements, msgBuffer, &msgLen) != 0) {
            if (pasynOctetSyncIO->write(pasynUserOctet_, msgBuffer, msgLen, DEVICE_RW_TIMEOUT, &nWrite) != asynSuccess)
                asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "Sending SetIntDelay failed\n");
        } else {
            asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "No meta index found for SetIntDelay\n");
        }
    } else if (function == setBp2_) {
        if (pStm->encodeU16IntegerArray("SetBP", value, nElements, msgBuffer, &msgLen) != 0) {
            if (pasynOctetSyncIO->write(pasynUserOctet_, msgBuffer, msgLen, DEVICE_RW_TIMEOUT, &nWrite) != asynSuccess)
                asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "Sending SetBP2 failed\n");
        } else {
            asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "No meta index found for SetBP2\n");
        }    
    } else {
        asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                  "%s::%s port %s invalid pasynUser->reason %d\n",
                  driverName, functionName, this->portName, function);
        return asynError;
    }
    return asynSuccess;
}
///////////////////////////////////////////////////////////////////////////////////

/*
**  asynFloat64 support
*/
asynStatus drvPulser::readFloat64 (asynUser *pasynUser, epicsFloat64 *value)
{
    int function = pasynUser->reason;
    static const char *functionName = "readFloat64";

    *value = 0;

    printf("%s called with function: %d\n", functionName, function);

    return asynSuccess;
}
///////////////////////////////////////////////////////////////////////////////////

asynStatus drvPulser::writeFloat64 (asynUser *pasynUser, epicsFloat64 value)
{
    int function = pasynUser->reason;
    static const char *functionName = "writeFloat64";
    
    printf("%s called with function: %d, value:%f\n", functionName, function, value);

    return asynSuccess;
}
///////////////////////////////////////////////////////////////////////////////////

/*
**  asynFloat32Array support
*/
asynStatus drvPulser::readFloat32Array(asynUser *pasynUser, epicsFloat32 *data, size_t maxChans, size_t *nactual)
{
    int function = pasynUser->reason;
    static const char *functionName = "readFloat32Array";

    *nactual = 0;

    printf("%s called with function: %d\n", functionName, function);

    return asynError;
}
///////////////////////////////////////////////////////////////////////////////////

asynStatus drvPulser::writeFloat32Array(asynUser *pasynUser, epicsFloat32 *value, size_t nElements)
{
    int function = pasynUser->reason;
    static const char *functionName = "writeFloat32Array";
    int msgLen;
    size_t nWrite;
    
    printf("%s called with function: %d, value[0]:%f, elements:%ld\n", functionName, function, value[0], nElements);

    for (unsigned int i=0; i<nElements; i++) {
        printf("value[%d]:%f\n", i, value[i]);
    }

    if (function == updateLimits_) {
        if (pStm->encodeFloat32Array("UpdateLimits", value, nElements, msgBuffer, &msgLen) != 0) {
            if (pasynOctetSyncIO->write(pasynUserOctet_, msgBuffer, msgLen, DEVICE_RW_TIMEOUT, &nWrite) != asynSuccess)
                asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "Sending UpdateLimits failed\n");
        } else {
            asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "No meta index found for UpdateLimits\n");
        }    
    } else if (function == setPulserCal_) {
        if (pStm->encodeFloat32Array("SetPulserCal", value, nElements, msgBuffer, &msgLen) != 0) {
            if (pasynOctetSyncIO->write(pasynUserOctet_, msgBuffer, msgLen, DEVICE_RW_TIMEOUT, &nWrite) != asynSuccess)
                asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "Sending SetPulsercal failed\n");
        } else {
            asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "No meta index found for SetPulserCal\n");
        }    
    } else {
        asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                  "%s::%s port %s invalid pasynUser->reason %d\n",
                  driverName, functionName, this->portName, function);
        return asynError;
    }
    return asynSuccess;
}
///////////////////////////////////////////////////////////////////////////////////

/*
**  asynOctet support
*/
asynStatus drvPulser::readOctet(asynUser *pasynUser, char *value, size_t maxChars, size_t *nactual, int *eomReason)
{
    int function = pasynUser->reason;
    static const char *functionName = "readOctet";

    //*nactual = 0;
    
    printf("%s called with function: %d\n", functionName, function);

    //return asynSuccess;
    return asynError;
}
///////////////////////////////////////////////////////////////////////////////////

asynStatus drvPulser::writeOctet (asynUser *pasynUser, const char *value, size_t maxChars, size_t *nActual)
{
    int function = pasynUser->reason;
    static const char *functionName = "writeOctet";
    int msgLen;
    size_t nWrite;
    string data(value, maxChars);

    printf("%s called with function: %d, value:%d\n", functionName, function, *value);

    if (function == setDateTime_) {
        if (pStm->encodeString("SetDateTime", data, msgBuffer, &msgLen) != 0) {
            if (pasynOctetSyncIO->write(pasynUserOctet_, msgBuffer, msgLen, DEVICE_RW_TIMEOUT, &nWrite) != asynSuccess)
                asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "Sending SetDateTime failed\n");
        } else {
            asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "No meta index found for SetDateTime\n");
        }    
    } else {
        asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                  "%s::%s port %s invalid pasynUser->reason %d\n",
                  driverName, functionName, this->portName, function);
        return asynError;
    }
    return asynSuccess;
}

int drvPulser::connectReadMetaData(char* octetPortName_)
{
    int eomReason;
    size_t nRead1, nRead2;
    int status;
    unsigned int msg_len = 0;
    char response[RESPONSE_SIZE];

    // connect to server with asynOctetSyncIO
    if (pasynOctetSyncIO->connect(octetPortName_, 0, &pasynUserOctet_, 0) == asynSuccess) {
        asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "Connected to LabVIEW server %s.\n", octetPortName_);
        isConnected_ = true;
    } else {
        asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "Can't connect to LabVIEW server %s.\n", octetPortName_);
        return 0;
    }

    // read meta data message length
    status = pasynOctetSyncIO->read(pasynUserOctet_, response, sizeof(unsigned int), DEVICE_RW_TIMEOUT, &nRead1, &eomReason);
    //printf("Meta length read status: %d, number of bytes:%d\n", status, (int)nRead1);
    if (nRead1 > 0) {
        msg_len = pStm->getMsgLen(response);
        printf("Meta length: %d\n", msg_len);
        
        // read meta data response
        status = pasynOctetSyncIO->read(pasynUserOctet_, response + sizeof(unsigned int), msg_len, DEVICE_RW_TIMEOUT, &nRead2, &eomReason);
        //printf("Meta data read status: %d, number of bytes:%d\n", status, (int)nRead2);
        if (nRead2 > 0) {
            if(pStm->decodeMetaData(response) > 0) {
                int num_meta, i;                                                                                                 
                string *meta_data;               

                num_meta = pStm->getNumMetaData();                                                                  
                meta_data = pStm->getMetaDataArray();                                                                    
                                                                                                                    
                for (i=0; i<num_meta; i++) {                                                                                                 
                    printf("MetaData[%d]: %s\n", i, meta_data[i].c_str());                                             
                }                                                                                                 
            } else {
                asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "Failed to process meta data\n");
            }
        } else {    
            asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "Failed to receive meta data\n");
        }
    } else {
        printf("Error in getting number of bytes in meta data message:%d\n", (int)nRead1);
    }
    
    return 1;
}    

int  drvPulser::sendReceive(char* commandStr, int value)
{
    int reqStatus = 0;
    int rspStatus = 0;
    int dcdStatus = 0;
    int msgLen;
    size_t nWrite;
    
    if (value != 0) {
        // send request
        if (pStm->encodeString(commandStr, "", msgBuffer, &msgLen) != 0) { 
            reqStatus = 1;
            if (pasynOctetSyncIO->write(pasynUserOctet_, msgBuffer, msgLen, DEVICE_RW_TIMEOUT, &nWrite) != asynSuccess) {
                asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "Sending %s request failed\n", commandStr);
                reqStatus = 0;
            }
        } else {
            asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "No meta index found for %s", commandStr);
            reqStatus = 0;
        }    
    
        if (reqStatus == 1)  {
            int eomReason;
            size_t nRead1, nRead2;
            char response[RESPONSE_SIZE];
            float* dataFloat;
            char* dataChar;
            int size;

            // read status message length
            rspStatus = pasynOctetSyncIO->read(pasynUserOctet_, response, sizeof(unsigned int), DEVICE_RW_TIMEOUT, &nRead1, &eomReason);
            //printf("%s message length status: %d, number of bytes:%d\n", commandStr, rspStatus, (int)nRead1);
            if (nRead1 > 0) {
                msgLen = pStm->getMsgLen(response);
                //printf("%s message length: %d\n", commandStr, msgLen);
    
                // read status data
                rspStatus = pasynOctetSyncIO->read(pasynUserOctet_, response + sizeof(unsigned int), msgLen, DEVICE_RW_TIMEOUT, &nRead2, &eomReason);
                //printf("%s data status: %d, number of bytes:%d\n", commandStr, rspStatus, (int)nRead2);
                if (nRead2 > 0) {
                    if (strcmp(commandStr, "Status") == 0)
                    {
                        //string dispstring;
                        //char smallstring[64];
                        
                        if((dcdStatus = pStm->decodeStatusMsg(&response[0], &dataFloat, &size)) != 0) {
                            //printf("Status decoded, size: %d.\n", size);
                            //for (int i=0; i<size; i++) {
                            //    sprintf(smallstring, "%.1f, ", dataFloat[i]);
                            //    dispstring += smallstring;
                            //    if (i>0 && !((i+1)%5))
                            //        dispstring += "\r\n";
                            //}
                            //dispstring += "\r\n";
                            //printf(dispstring.c_str());

                            //update status PV
                            rspStatus = doCallbacksFloat32Array(dataFloat, size, status_, 0);
                            //printf("doCallbacks status: %d\n", rspStatus);
                            
                            //callParamCallbacks();
                            
                            delete dataFloat;
                        }
                    }
                    else if (strcmp(commandStr, "DateTime") == 0)
                    {
                        string dateTimeStr;
                        
                        if((dcdStatus = pStm->decodeDateTimeMsg(&response[0], &dataChar, &size)) != 0) {
                            dateTimeStr.append(dataChar, size);
                            
                            //printf("DateTime: %s\n", dateTimeStr.c_str());
                            
                            //update DataTime PV
                            setStringParam(dateTime_, dateTimeStr);
                            
                            // do callbacks so higher layers see any changes
                            callParamCallbacks();

                            delete dataChar;
                        }
                    }
                    
                    if (dcdStatus == 0)
                    {
                        asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "Failed to decode %s data\n", commandStr);
                    }
                } else {    
                    asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "Failed to get %s data\n", commandStr);
                }
            } else {
                asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "Error in getting number of bytes: %d\n", (int)nRead1);
                //printf("Error in getting number of bytes: %d\n", (int)nRead1);
            }
        }
    }
    
    return 1;
}

/*
****************************************************************************
** Poller thread for port reads
   One instance spawned per asyn port
****************************************************************************
*/

static void pollerThreadC(void *drvPvt)
{
    drvPulser *pPvt = (drvPulser *)drvPvt;

    pPvt->pollerThread();
}

void drvPulser::pollerThread()
{
    asynStatus prevIOStatus = asynSuccess;
    epicsTimeStamp currTime, cycleTimeFiveSec, cycleTimeTenSec;
    double dTFiveSec, dTTenSec;

    printf("poller thread called!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");

    lock();

    /* Loop forever */
    while (1)
    {
        /* Sleep for the poll delay or waiting for epicsEvent with the port unlocked */
        unlock();

        epicsEventWaitWithTimeout(pollerEventId_, pollTime_);

        if (pulserExiting_) break;

        epicsTimeGetCurrent(&currTime);
        dTFiveSec = epicsTimeDiffInSeconds(&currTime, &cycleTimeFiveSec);
        dTTenSec = epicsTimeDiffInSeconds(&currTime, &cycleTimeTenSec);

        /* Lock the port.  It is important that the port be locked so other threads cannot access the Pulser
         * structure while the poller thread is running. */
        lock();

        if(dTFiveSec >= 5.) {

            /*Update cycle time*/
            epicsTimeGetCurrent(&cycleTimeFiveSec);
        }

        if(dTTenSec >= 10.) {

            /*Update cycle time*/
            epicsTimeGetCurrent(&cycleTimeTenSec);
        }

        /*****************Do this every cycle******************************/
        /* If we have an I/O error this time and the previous time, just try again */
        if (ioStatus_ != asynSuccess &&
            ioStatus_ == prevIOStatus) {
            epicsThreadSleep(1.0);
            continue;
        }

        /* If the I/O status has changed then force callbacks */
        if (ioStatus_ != prevIOStatus) 
            forceCallback_ = true;

        /* Don't start polling until EPICS interruptAccept flag is set,
         * because it does callbacks to device support. */
        while (!interruptAccept) {
            unlock();
            epicsThreadSleep(0.1);
            lock();
        }

        //unlock();
        /* Reset the forceCallback flag */
        forceCallback_ = false;

        /* Set the previous I/O status */
        prevIOStatus = ioStatus_;
    }
}

/*
**  User functions
*/

extern "C" {
/*
** drvPulserConfigure() - create and init an asyn port driver for a Pulser
*/

/** EPICS iocsh callable function to call constructor for the drvPulser class. */
asynStatus drvPulserConfigure(const char *portName, const char *hostInfo)
{
    if (!portName || !hostInfo)
        return asynError;
    
    new drvPulser(portName, hostInfo);
    
    return asynSuccess;
}

//==========================================================//
// IOCsh functions here
//==========================================================//
static void drvPulserConfigureCallFunc(const iocshArgBuf* args) {
    const char *portName = args[0].sval;
    const char *ip = args[1].sval;
    int port = args[2].ival;

    if (!portName) {
        epicsPrintf("Invalid port name passed.\n");
        return;
    }

    if (!ip) {
        epicsPrintf("Invalid IP passed.\n");
        return;
    }

    if (port <= 0) {
        epicsPrintf("The port %i is invalid.\n", port);
        return;
    }
    
    char hostInfo[64];
    sprintf(hostInfo,"%s:%i TCP", ip, port);

    drvPulserConfigure(portName, hostInfo);
}


int drvPulserRegister() {
    
    /* drvPulserConfigure("ASYN_PORT", "IP", PORT_NUMBER) */
    {
        static const iocshArg arg1 = {"Port Name", iocshArgString};
        static const iocshArg arg2 = {"IP", iocshArgString};
        static const iocshArg arg3 = {"Port Number", iocshArgInt};
        static const iocshArg* const args[] = {&arg1, &arg2, &arg3};
        static const iocshFuncDef func = {"drvPulserConfigure", 3, args};
        iocshRegister(&func, drvPulserConfigureCallFunc);
    }
    
    return 0;
}
epicsExportRegistrar(drvPulserRegister);

}// extern "C"