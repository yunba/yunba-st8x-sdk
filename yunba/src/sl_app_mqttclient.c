#include "sl_type.h"
#include "sl_debug.h"
#include "sl_error.h"
#include "sl_stdlib.h"
#include "sl_system.h"
#include "sl_uart.h"
#include "sl_tcpip.h"
#include "sl_os.h"
#include "sl_timer.h"
#include "sl_app_event.h"
#include "MQTTPacket.h"
#include "sl_app_mqttclient.h"

#define BUF_SIZE 256

//#define SERVER_IP "182.92.205.77";
//#define U16 SERVER_PORT 9999;
#define SERVER_IP "182.92.106.18" /* abj-front-2 */
#define SERVER_PORT 1883

#define CLIENT_ID "0000002823-000000054812"
#define USERNAME "2770382004042745344"
#define PASSWORD "eb53f628fcf87"

static U8 gSendBuf[BUF_SIZE];
static U8 gRecvBuf[BUF_SIZE];

static S32 gSocketId = 0;
static HANDLE gTask;

static void SendMsg(U32 ulMsgId, U32 ulParam) {
    SL_EVENT stEvnet;
    SL_TASK hTask;

    SL_Memset(&stEvnet, 0, sizeof(SL_EVENT));
    stEvnet.nEventId = ulMsgId;
    stEvnet.nParam1 = ulParam;
    hTask.element[0] = gTask;
    SL_SendEvents(hTask, &stEvnet);
}

static void SendPacket(U8 *data, U32 len) {
    S32 slRet = 0;
    while (len > 0) {
        slRet = SL_TcpipSocketSend(gSocketId, data, len);
        if (slRet <= 0) {
            //SL_ApiPrint("SLAPP: SendPacket socket send fail ret=%d", slRet);
            SendMsg(EVT_APP_MQTT_ERROR, 0);
        } else {
            len -= slRet;
        }
    }
}

static void HandlePacket(U8 *data, S32 len) {
    U8 dup;
    S32 qos;
    U8 retained;
    U64 id;
    EXTED_CMD cmd;
    S32 status;
    U8 *payload;
    S32 payloadLen;
    MQTTString topicName;
    U8 packetType;

    packetType = data[0] >> 4;
    //SL_ApiPrint("HandlePacket: type: %d", packetType);
    /* TODO: finish other types */
    switch (packetType) {
        case CONNACK:
            SendMsg(EVT_APP_MQTT_CONNACK, 0);
            break;
        case PUBLISH:
            if (MQTTDeserialize_publish(&dup, &qos, &retained, &id, &topicName,
                                        &payload, &payloadLen, data, len) != 1) {
                //SL_ApiPrint("MQTTDeserialize_publish error");
            } else {
                payload[payloadLen] = 0;
                SendMsg(EVT_APP_MQTT_PUBLISH, payload);
            }
            if (qos != 0) {
                if (qos == 1) {
                    len = MQTTSerialize_ack(gSendBuf, BUF_SIZE, PUBACK, 0, id);
                } else if (qos == 2) {
                    len = MQTTSerialize_ack(gSendBuf, BUF_SIZE, PUBREC, 0, id);
                }
                if (len <= 0) {
                    //SL_ApiPrint("MQTTSerialize_ack error");
                } else {
                    SendPacket(gSendBuf, len);
                }
            }
            break;
        case EXTCMD:
            if (MQTTDeserialize_extendedcmd(&dup, &qos, &retained, &id, &cmd, &status, &payload, &payloadLen, data,
                                            len) != 1) {
                //SL_ApiPrint("MQTTDeserialize_extendedcmd error");
            } else {
                payload[payloadLen] = 0;
                SendMsg(EVT_APP_MQTT_EXTCMD, payload);
            }
            break;
        case PUBACK:
        case SUBACK:
        case PUBREC:
        case PUBCOMP:
        case PUBREL:
        case PINGRESP:
            break;
    }
}

static void MQTTGprsNetActRsp(U8 ucCidIndex, S32 slErrorCode) {
    S32 slRet = 0;
    //SL_ApiPrint("MQTTGprsNetActRsp: %d", slErrorCode);
    if (slErrorCode != SL_RET_OK) {
        return;
    }
    slRet = SL_TcpipSocketCreate(gSocketId, SL_TCPIP_SOCK_TCP);
    if (slRet < 0) {
        //SL_ApiPrint("SLAPP: socket create SockId[%d] fail[%d]", gSocketId, slRet);
        return;
    }

    //SL_ApiPrint("SL_TcpipSocketCreate OK, sock id[%d]", slRet);
    slRet = SL_TcpipSocketBind(gSocketId);
    if (slRet != SL_RET_OK) {
        //SL_ApiPrint("SLAPP: socket bind fail[%d]", slRet);
        return;
    }

    SL_Sleep(1000);

    slRet = SL_TcpipSocketConnect(gSocketId, SERVER_IP, SERVER_PORT);
    //SL_ApiPrint("SLAPP: SL_TcpipSocketConnect ret[%d]", slRet);
}

static void MQTTTcpipConnRsp(U8 ucCidIndex, S32 slSockId, BOOL bResult, S32 slErrorCode) {
    //SL_ApiPrint("MQTTTcpipConnRsp: %d", slErrorCode);
    if (bResult == FALSE) {
        //SL_ApiPrint("MQTTTcpipConnRsp: socket connect fail[%d]", slErrorCode);
        SendMsg(EVT_APP_MQTT_ERROR, 0);
        return;
    }

    SendMsg(EVT_APP_MQTT_INIT_OK, 0);
}

static void MQTTTcpipSendRsp(U8 ucCidIndex, S32 slSockId, BOOL bResult, S32 slErrorCode) {
    //SL_ApiPrint("MQTTGprsNetActRsp: %d, %d", slSockId, slErrorCode);
    if (bResult == FALSE) {
        //SL_ApiPrint("SLAPP: socket send fail[%d]", slErrorCode);
        return;
    }
}

static void MQTTTcpipRcvRsp(U8 ucCidIndex, S32 slSockId, BOOL bResult, S32 slErrorCode) {
    S32 slRet = 0;
    U8 ch;
    S32 multiplier = 1;
    S32 remain_len = 0;
    S32 len = 0;

    //SL_ApiPrint("MQTTTcpipRcvRsp: %d, %d", slSockId, slErrorCode);
    if (bResult == FALSE) {
        //SL_ApiPrint("SLAPP: socket receive fail[%d]", slErrorCode);
        return;
    }

    /* first byte */
    slRet = SL_TcpipSocketRecv(slSockId, gRecvBuf + len, 1);
    if (slRet != 1) {
        //SL_ApiPrint("SLAPP: socket receive fail, ret[%d]", slRet);
        SendMsg(EVT_APP_MQTT_ERROR, 0);
        return;
    }
    //SL_ApiPrint("MQTTTcpipRcvRsp: first byte: %d", gRecvBuf[len]);
//    SL_MEMBLOCK(gRecvBuf, slRet, 16);
    len += 1;

    /* remaining length */
    do {
        slRet = SL_TcpipSocketRecv(slSockId, &ch, 1);
        if (slRet != 1) {
            //SL_ApiPrint("SLAPP: socket receive fail, ret[%d]", slRet);
            SendMsg(EVT_APP_MQTT_ERROR, 0);
            return;
        }
//        SL_MEMBLOCK(&ch, slRet, 16);
        gRecvBuf[len] = ch;
        len += 1;
        remain_len += (ch & 127) * multiplier;
        multiplier *= 128;
    } while ((ch & 128) != 0);

    //SL_ApiPrint("MQTTTcpipRcvRsp: remaining length: %d", remain_len);
    /* rest data */
    if (remain_len > 0) {
        slRet = SL_TcpipSocketRecv(slSockId, gRecvBuf + len, remain_len);
        if (slRet != remain_len) {
            //SL_ApiPrint("SLAPP: socket receive fail, ret[%d]", slRet);
            SendMsg(EVT_APP_MQTT_ERROR, 0);
            return;
        }
        //SL_ApiPrint("MQTTTcpipRcvRsp: rest data:");
//        SL_MEMBLOCK(gRecvBuf + len, slRet, 16);
        len += remain_len;
    }
    //SL_ApiPrint("MQTTTcpipRcvRsp: finished");

    HandlePacket(gRecvBuf, len);
}

static void MQTTTcpipCloseRsp(U8 ucCidIndex, S32 slSockId, BOOL bResult, S32 slErrorCode) {
    S32 slRet = 0;
    S32 slState = 0;

    //SL_ApiPrint("MQTTTcpipCloseRsp: %d, %d", slSockId, slErrorCode);
    if (bResult == FALSE) {
        //SL_ApiPrint("SLAPP: socket Close fail[%d]", slErrorCode);
        return;
    }
    //SL_ApiPrint("SLAPP: socket[%d] Close OK", slSockId);

    slState = SL_TcpipGetState(slSockId);
    //SL_ApiPrint("SLAPP: MQTTTcpipCloseRsp socket state[%d]", slState);

    slRet = SL_TcpipGprsNetDeactive();
    if (slRet != SL_RET_OK) {
        //SL_ApiPrint("SLAPP: gprs net deact fail[0x%x]", slRet);
    }
}

static void MQTTGprsNetDeactRsp(U8 ucCidIndex, S32 slErrorCode) {
    //SL_ApiPrint("MQTTGprsNetDeactRsp: %d", slErrorCode);
    if (slErrorCode != SL_RET_OK) {
        //SL_ApiPrint("SLAPP: gprs net deact rsp fail[%d]", slErrorCode);
        return;
    }
    //SL_ApiPrint("SLAPP: gprs net deact rsp OK");
}

void MQTTInit(HANDLE stTask) {
    S32 slRet = 0;
    SL_TCPIP_CALLBACK stSlTcpipCb;

    gTask = stTask;
    SL_Memset(&stSlTcpipCb, 0, sizeof(stSlTcpipCb));
    stSlTcpipCb.pstSlnetAct = MQTTGprsNetActRsp;
    stSlTcpipCb.pstSlnetDea = MQTTGprsNetDeactRsp;
    stSlTcpipCb.pstSlsockConn = MQTTTcpipConnRsp;
    stSlTcpipCb.pstSlsockSend = MQTTTcpipSendRsp;
    stSlTcpipCb.pstSlsockRecv = MQTTTcpipRcvRsp;
    stSlTcpipCb.pstSlsockClose = MQTTTcpipCloseRsp;
    SL_TcpipSetRetrTime(2);
    SL_TcpipGprsNetInit(0, &stSlTcpipCb);

    SL_TcpipGprsApnSet("CMNET", "S80", "S80");
    slRet = SL_TcpipGprsNetActive();
    if (slRet != SL_RET_OK) {
        //SL_ApiPrint("SLAPP: gprs net act fail[%d]", slRet);
    }
    //SL_ApiPrint("SLAPP: MQTTInit OK!");
}

void MQTTUnInit();

void MQTTConnect() {
    S32 slRet = 0;

    MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
    data.willFlag = 0;
    data.MQTTVersion = 19;
    data.clientID.cstring = CLIENT_ID;
    data.username.cstring = USERNAME;;
    data.password.cstring = PASSWORD;
    data.keepAliveInterval = 200;
    data.cleansession = 0;

    slRet = MQTTSerialize_connect(gSendBuf, BUF_SIZE, &data);
    if (slRet <= 0) {
        //SL_ApiPrint("MQTTSerialize_connect: %d", slRet);
        SendMsg(EVT_APP_MQTT_ERROR, 0);
        return;
    }

    SendPacket(gSendBuf, slRet);
}

void MQTTDisconnect();

void MQTTPublish(char *topic, U8 *payload) {
    S32 slRet = 0;
    MQTTString strTopic = MQTTString_initializer;
    strTopic.cstring = (char *) topic;

    slRet = MQTTSerialize_publish(gSendBuf, BUF_SIZE, 0, 1, 0, SL_TmGetTick(),
                                  strTopic, payload, strlen(payload));
    if (slRet <= 0) {
        //SL_ApiPrint("MQTTSerialize_publish: %d", slRet);
        SendMsg(EVT_APP_MQTT_ERROR, 0);
        return;
    }

    SendPacket(gSendBuf, slRet);
}

void MQTTPingreq() {
    S32 slRet = 0;

    slRet = MQTTSerialize_pingreq(gSendBuf, BUF_SIZE);
    if (slRet <= 0) {
        //SL_ApiPrint("MQTTSerialize_pingreq: %d", slRet);
        SendMsg(EVT_APP_MQTT_ERROR, 0);
        return;
    }

    SendPacket(gSendBuf, slRet);
}

void MQTTSubscribe(char *topic);

void MQTTUnSubscribe(char *topic);

void MQTTSetAlias(char *alias) {
    MQTTPublish(",yali", alias);
}