#ifndef stm_msg_H
#define stm_msg_H

#include <string>
#include <string.h>
using std::string;

/* This class implements an STM client using TCP/IP.  As of this time, the class does not have the ability to
act as a server */
class StmMsg
{
public:
    StmMsg(void);
    ~StmMsg(void);
    int decodeMetaData(char* buffer);
    string *getMetaDataArray();
    int getNumMetaData();
    int getIdFromName(string Name, unsigned short *MetaID, int *found); //Maps a message id into a meta element
    int encodeU16Integer(string msgMeta, unsigned short toEncode, char* msgBuffer, int* msgLen);
    int encodeU16IntegerArray(string msgMeta, short* toEncode, int nElements, char* msgBuffer, int* msgLen);
    int encodeBoolean(string msgMeta, char toEncode, char* msgBuffer, int* msgLen);
    int encodeFloat32Array(string msgMeta, float* toEncode, int nElements, char* msgBuffer, int* msgLen);
    int encodeString(string msgMeta, string data, char* msgBuffer, int* msgLen);
    int decodeStatusMsg(char* msg, float** data, int* size);
    int decodeDateTimeMsg(char* msg, char** data, int* size);
    unsigned int getMsgLen(char* msg);

private:
    string *m_metaData;
    int m_numMeta;
    int m_metaDefined;
};

#endif /* stm_msg_H */
