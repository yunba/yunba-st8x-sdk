#include "sl_type.h"
#include "sl_debug.h"
#include "sl_error.h"
#include "sl_stdlib.h"
#include "sl_system.h"
#include "sl_uart.h"
#include "sl_tcpip.h"
#include "sl_os.h"
#include "sl_timer.h"
#include "sl_app_mqttclient.h"

static U8 *gServerIp = "182.92.205.77";
static U16 gServerPort = 9999;
static S32 gSocketId = 0;

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
    }
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

void MQTTConnect() {
    S32 slRet = 0;
    SL_TCPIP_CALLBACK stSlTcpipCb;

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
    SL_ApiPrint("SLAPP: MQTTConnect OK!");
}

void MQTTDisconnect();

void MQTTPublish(const char *topic, const U8 *payload);

void MQTTSubscribe(const char *topic);

void MQTTUnsubscribe(const char *topic);

void MQTTSetAlias(const char *alias);