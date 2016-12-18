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
static U8 gSendBuf[BUF_SIZE];

static U8 *gServerIp = "182.92.106.18";
static U16 gServerPort = 1883;
//static U8 *gServerIp = "182.92.205.77";
//static U16 gServerPort = 9999;
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

static void MQTTGprsNetActRsp(U8 ucCidIndex, S32 slErrorCode) {
    S32 slRet = 0;
    SL_ApiPrint("MQTTGprsNetActRsp: %d", slErrorCode);
    if (slErrorCode != SL_RET_OK) {
        return;
    }
    slRet = SL_TcpipSocketCreate(gSocketId, SL_TCPIP_SOCK_TCP);
    if (slRet < 0) {
        SL_ApiPrint("SLAPP: socket create SockId[%d] fail[%d]", gSocketId, slRet);
        return;
    }

    SL_ApiPrint("SL_TcpipSocketCreate OK, sock id[%d]", slRet);
    slRet = SL_TcpipSocketBind(gSocketId);
    if (slRet != SL_RET_OK) {
        SL_ApiPrint("SLAPP: socket bind fail[%d]", slRet);
        return;
    }

    SL_Sleep(1000);

    slRet = SL_TcpipSocketConnect(gSocketId, gServerIp, gServerPort);
    SL_ApiPrint("SLAPP: SL_TcpipSocketConnect ret[%d]", slRet);
}

static void MQTTTcpipConnRsp(U8 ucCidIndex, S32 slSockId, BOOL bResult, S32 slErrorCode) {
    SL_ApiPrint("MQTTTcpipConnRsp: %d", slErrorCode);
    if (bResult == FALSE) {
        SL_ApiPrint("MQTTTcpipConnRsp: socket connect fail[%d]", slErrorCode);
        SendMsg(EVT_APP_MQTT_ERROR, 0);
        return;
    }

    SendMsg(EVT_APP_MQTT_INIT_OK, 0);
}

static void MQTTTcpipSendRsp(U8 ucCidIndex, S32 slSockId, BOOL bResult, S32 slErrorCode) {
    SL_ApiPrint("MQTTGprsNetActRsp: %d, %d", slSockId, slErrorCode);
    if (bResult == FALSE) {
        SL_ApiPrint("SLAPP: socket send fail[%d]", slErrorCode);
        return;
    }
}

static void MQTTTcpipRcvRsp(U8 ucCidIndex, S32 slSockId, BOOL bResult, S32 slErrorCode) {
    S32 slRet = 0;
    U8 data[256] = {0};

    SL_ApiPrint("MQTTTcpipRcvRsp: %d, %d", slSockId, slErrorCode);
    if (bResult == FALSE) {
        SL_ApiPrint("SLAPP: socket receive fail[%d]", slErrorCode);
        return;
    }

    slRet = SL_TcpipSocketRecv(slSockId, data, sizeof(data));
    if (slRet >= 0) {
        SL_ApiPrint("SLAPP: socket[%d] receive OK, length[%d], data[%s]", slSockId, slRet, data);
        SL_MEMBLOCK(data, slRet, 16);
    } else {
        SL_ApiPrint("SLAPP: socket receive fail, ret[%d]", slRet);
    }
}

static void MQTTTcpipCloseRsp(U8 ucCidIndex, S32 slSockId, BOOL bResult, S32 slErrorCode) {
    S32 slRet = 0;
    S32 slState = 0;

    SL_ApiPrint("MQTTTcpipCloseRsp: %d, %d", slSockId, slErrorCode);
    if (bResult == FALSE) {
        SL_ApiPrint("SLAPP: socket Close fail[%d]", slErrorCode);
        return;
    }
    SL_ApiPrint("SLAPP: socket[%d] Close OK", slSockId);

    slState = SL_TcpipGetState(slSockId);
    SL_ApiPrint("SLAPP: MQTTTcpipCloseRsp socket state[%d]", slState);

    slRet = SL_TcpipGprsNetDeactive();
    if (slRet != SL_RET_OK) {
        SL_ApiPrint("SLAPP: gprs net deact fail[0x%x]", slRet);
    }
}

static void MQTTGprsNetDeactRsp(U8 ucCidIndex, S32 slErrorCode) {
    SL_ApiPrint("MQTTGprsNetDeactRsp: %d", slErrorCode);
    if (slErrorCode != SL_RET_OK) {
        SL_ApiPrint("SLAPP: gprs net deact rsp fail[%d]", slErrorCode);
        return;
    }
    SL_ApiPrint("SLAPP: gprs net deact rsp OK");
}

static void sendPacket(U8 *data, U32 len) {
    S32 slRet = 0;
    while (len > 0) {
        slRet = SL_TcpipSocketSend(gSocketId, data, len);
        if (slRet <= 0) {
            SL_ApiPrint("SLAPP: SL_AppTcpipRsnd socket send fail ret=%d", slRet);
            SendMsg(EVT_APP_MQTT_ERROR, 0);
        } else {
            len -= slRet;
        }
    }
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
        SL_ApiPrint("SLAPP: gprs net act fail[%d]", slRet);
    }
    SL_ApiPrint("SLAPP: MQTTInit OK!");
}

void MQTTUnInit();

void MQTTConnect() {
    S32 slRet = 0;

    MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
    data.willFlag = 0;
    data.MQTTVersion = 19;
    data.clientID.cstring = "0000002823-000000054812";
    data.username.cstring = "2770382004042745344";
    data.password.cstring = "eb53f628fcf87";

    data.keepAliveInterval = 200;
    data.cleansession = 0;
    slRet = MQTTSerialize_connect(gSendBuf, BUF_SIZE, &data);
    if (slRet <= 0) {
        SL_ApiPrint("MQTTSerialize_connect: %d", slRet);
        SendMsg(EVT_APP_MQTT_ERROR, 0);
        return;
    }

    sendPacket(gSendBuf, slRet);
}

void MQTTDisconnect();

void MQTTPublish(const char *topic, const U8 *payload);

void MQTTSubscribe(const char *topic);

void MQTTUnSubscribe(const char *topic);

void MQTTSetAlias(const char *alias);