#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>

#include "stmConv.h"
#include "stmMsg.h"

StmMsg::StmMsg(void)
{
    m_metaDefined = 0;
}

StmMsg::~StmMsg(void)
{
}

int StmMsg::decodeMetaData(char* buffer)
{
    int metaElementLen = 0, x;
    char *placeHolder;
    int retVal = 0;
   
    // skip first 4 bytes: length of message
    placeHolder = buffer + sizeof(unsigned int);
    
    //  next 4 characters: how many elements are in meta data message
    m_numMeta = intUnflatten(placeHolder);
    
    if(m_numMeta > 0)
    {
        // allocate memory for the Meta Data
        m_metaData = new string[m_numMeta]; 
    
        // placeHolder keeps track of where you are in receive buffer
        placeHolder = placeHolder + 4;
    
        for(x = 0; x < m_numMeta; x++)
        {
            // length of the Meta Element about to be read
            metaElementLen = intUnflatten(placeHolder);
            placeHolder = placeHolder + 4;      
            m_metaData[x] = string(placeHolder, metaElementLen);
            placeHolder = placeHolder + metaElementLen;
        }

        m_metaDefined = 1;
        retVal = 1;
    }
    
    return retVal;
}

string *StmMsg::getMetaDataArray()
{
    return m_metaData;
}

int StmMsg::getNumMetaData()
{
    return m_numMeta;
}

int StmMsg::getIdFromName(string name, unsigned short *metaID, int *found)
{
    unsigned short x;
    *found = 0;

    for (x = 0; x < m_numMeta; x++)
    {
        if (name == m_metaData[x])
        {
            *found = 1;
            *metaID = x;
        }
    }
    
    return 0;
}

int StmMsg::encodeBoolean(string msgMeta, char toEncode, char* msgBuffer, int* msgLen)
{
    unsigned short int id = 0;
    int found = 0;
    int size = 0;
    char *actPtr;

    // get meta index  
    getIdFromName(msgMeta, &id, &found);
    
    if(found)
    {
        // encode size: size of meta index + size of char (message size doesn't include the size itself)
        size = sizeof(unsigned short) + sizeof(char);
        
        actPtr = msgBuffer;

       // flatten message size into buffer
        intFlatten(&size, actPtr);

        actPtr += sizeof(int);
        
        // flatten meta index into buffer
        ushortFlatten(&id, actPtr);
        
        actPtr += sizeof(unsigned short);
        
         // encode char: no need to flatten as it's a char
        *actPtr = toEncode;

        // actual message length includes encoded size of message at first 2 bytes
        *msgLen = sizeof(unsigned int) + size;
        
        return 1;
    }
    else
    {
        return 0;
    }
}

int StmMsg::encodeU16Integer(string msgMeta, unsigned short toEncode, char* msgBuffer, int* msgLen)
{
    unsigned short int id = 0;
    int found = 0;
    int size = 0;
    char *actPtr;

    // get meta index  
    getIdFromName(msgMeta, &id, &found);
    
    if(found)
    {
        // encode size: size of meta index + size of unsigned short (message size doesn't include the size itself)
        size = 2 * sizeof(unsigned short);
        
        actPtr = msgBuffer;

        // flatten message size into buffer
        intFlatten(&size, actPtr);

        actPtr += sizeof(int);
        
        // flatten meta index into buffer
        ushortFlatten(&id, actPtr);
        
        actPtr += sizeof(unsigned short);
        
        // flatten number into buffer
        ushortFlatten(&toEncode, actPtr);

        // actual message length includes encoded size of message at first 2 bytes
        *msgLen = sizeof(unsigned int) + size;
        
        return 1;
    }
    else
    {
        return 0;
    }
}

int StmMsg::encodeU16IntegerArray(string msgMeta, short* toEncode, int nElements, char* msgBuffer, int* msgLen)
{
    unsigned short int id = 0;
    int found = 0;
    int size = 0;
    char *actPtr;

    // get meta index  
    getIdFromName(msgMeta, &id, &found);
    
    if(found)
    {
        // encode size: size of meta index + size of array size + 
        // array size * size of short numbers (message size doesn't include the size itself)
        size = sizeof(unsigned short) + sizeof(unsigned int) + nElements * sizeof(unsigned short);
        
        actPtr = msgBuffer;

        // flatten message size into buffer
        intFlatten(&size, actPtr);

        actPtr += sizeof(int);
        
        // flatten meta index into buffer
        ushortFlatten(&id, actPtr);
        
        actPtr += sizeof(unsigned short);
        
        // flatten array data into buffer
        shortArrFlatten(toEncode, actPtr, nElements);

        // actual message length includes encoded size of message at first 2 bytes
        *msgLen = sizeof(unsigned int) + size;
        
        return 1;
    }
    else
    {
        return 0;
    }
}

int StmMsg::encodeFloat32Array(string msgMeta, float* toEncode, int nElements, char* msgBuffer, int* msgLen)
{
    unsigned short int id = 0;
    int found = 0;
    int size = 0;
    char *actPtr;

    // get meta index  
    getIdFromName(msgMeta, &id, &found);
    
    if(found)
    {
        // encode size: size of meta index + size of array size + 
        // array size * size of float numbers (message size doesn't include the size itself)
        size = sizeof(unsigned short) + sizeof(unsigned int) + nElements * sizeof(float);
        
        actPtr = msgBuffer;

        // flatten message size into buffer
        intFlatten(&size, actPtr);

        actPtr += sizeof(int);
        
        // flatten meta index into buffer
        ushortFlatten(&id, actPtr);
        
        actPtr += sizeof(unsigned short);
        
        // flatten array data into buffer
        sglArrFlatten(toEncode, actPtr, nElements);

        // actual message length includes encoded size of message at first 2 bytes
        *msgLen = sizeof(unsigned int) + size;
        
        return 1;
    }
    else
    {
        return 0;
    }
}

int StmMsg::encodeString(string msgMeta, string data, char* msgBuffer, int* msgLen)
{
    unsigned short int id = 0;
    int found = 0;
    int size = 0;
    char *actPtr;

    // get meta index  
    getIdFromName(msgMeta, &id, &found);
    
    if(found)
    {
        // encode size: size of meta index + size of char (message size doesn't include the size itself)
        size = sizeof(unsigned short) + sizeof(unsigned int) + data.length();
        
        actPtr = msgBuffer;

        // flatten message size into buffer
        intFlatten(&size, actPtr);

        actPtr += sizeof(int);
        
        // flatten meta index into buffer
        ushortFlatten(&id, actPtr);
        
        actPtr += sizeof(unsigned short);
        
         // encode string
        stringFlatten(data, actPtr);
        
        // actual message length includes encoded size of message at first 2 bytes
        *msgLen = sizeof(unsigned int) + size;
        
        return 1;
    }
    else
    {
        return 0;
    }
}

int StmMsg::decodeStatusMsg(char* msg, float** data, int* size)
{
    int metaindex;
    string msgMeta;
    
    // get meta index: message ptr + size of message size
    metaindex = ushortUnflatten(msg + sizeof(unsigned int));
                
    // get meta string from meta buffer by index
    msgMeta = m_metaData[metaindex];
                
    if (msgMeta == "Status")
    {
        //printf("Status index found\n");
        *data = sglArrUnflatten(msg + sizeof(unsigned int) + sizeof(unsigned short), size);
    }
    else
    {
        //printf("Error in Status decoding, index: %d, metadata: %s\n", metaindex, msgMeta.c_str());
        return 0;
    }
  
  return 1;    
}

int StmMsg::decodeDateTimeMsg(char* msg, char** data, int* size)
{
    int metaindex;
    string msgMeta;
    
    // get meta index: message ptr + size of message size
    metaindex = ushortUnflatten(msg + sizeof(unsigned int));
                
    // get meta string from meta buffer by index
    msgMeta = m_metaData[metaindex];
                
    if (msgMeta == "DateTime")
    {
        //printf("DateTime index found\n");
        *data = stringUnflatten(msg + sizeof(unsigned int) + sizeof(unsigned short), size);
    }
    else
    {
        //printf("Error in DateTime decoding, index: %d, metadata: %s\n", metaindex, msgMeta.c_str());
        return 0;
    }
  
  return 1;    
}

unsigned int StmMsg::getMsgLen(char* msg)
{
    return uintUnflatten(msg);
}
