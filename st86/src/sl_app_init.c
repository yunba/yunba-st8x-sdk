#include "sl_os.h"
#include "sl_stdlib.h"
#include "sl_type.h"
#include "sl_debug.h"
#include "sl_error.h"
#include "sl_sms.h"
#include "sl_call.h"
#include "sl_timer.h"
#include "sl_system.h"
#include "sl_memory.h"
#include "sl_uart.h"
#include "sl_tcpip.h"
#include "sl_gpio.h"
#include "sl_app_event.h"
#include "cJSON.h"
#include "sl_app_mqttclient.h"

#define ALIAS "yunba_lock_test"
#define REPORT_TOPIC "yunba_lock_report"

#define APP_TASK_DEVICE_STACK_SIZE    (2 * 2048)
#define APP_TASK_DEVICE_PRIORITY      (SL_APP_TASK_PRIORITY_BEGIN + 1)
#define APP_TASK_YUNBA_STACK_SIZE     (2 * 2048)
#define APP_TASK_YUNBA_PRIORITY       (SL_APP_TASK_PRIORITY_BEGIN + 2)

#define GPIO_SWITCH_1   SL_GPIO_1
#define GPIO_SWITCH_2   SL_GPIO_2
#define GPIO_BUZZER     SL_GPIO_5
#define GPIO_MOTOR      SL_GPIO_6

#define TEST_TIME_ID 200
#define CHECK_SWITCH_1_TIME_ID 201
#define CHECK_SWITCH_2_TIME_ID 202
#define MQTT_KEEPALIVE_TIME_ID 203

typedef enum {
    LOCK_UNLOCKED,
    LOCK_LOCKED,
    LOCK_UNLOCKING,
    LOCK_LOCKING
} LOCK_STATUS;

/* variable */
HANDLE g_SLAppDevice = HNULL;
HANDLE g_SLAppYunba = HNULL;

static U8 gucUartRecvBuff[100] = {0};
static U8 gucUartRecvBuffCurPosi = 0;

void SL_AppSendMsg(HANDLE stTask, U32 ulMsgId, U32 ulParam) {
    SL_EVENT stEvnet;
    SL_TASK hTask;

    SL_Memset(&stEvnet, 0, sizeof(SL_EVENT));
    stEvnet.nEventId = ulMsgId;
    stEvnet.nParam1 = ulParam;
    hTask.element[0] = stTask;
    SL_SendEvents(hTask, &stEvnet);
}

void SL_AppHandleYunbaMsg(U8 *data) {
    cJSON *root = cJSON_Parse(data);
    if (root) {
        int ret_size = cJSON_GetArraySize(root);
        if (ret_size >= 1) {
            if (strcmp(cJSON_GetObjectItem(root, "cmd")->valuestring, "unlock") == 0) {
                SL_AppSendMsg(g_SLAppDevice, EVT_APP_UNLOCK, 0);
            }
        }
        cJSON_Delete(root);
    }
}

void SL_AppReportStatus(S32 lockState) {
    cJSON *status;
    char *json;

    status = cJSON_CreateObject();
    cJSON_AddStringToObject(status, "alias", ALIAS);
    cJSON_AddBoolToObject(status, "lock", lockState == LOCK_LOCKED);

    json = cJSON_PrintUnformatted(status);
    SL_ApiPrint("status: %s", json);

    MQTTPublish(REPORT_TOPIC, json);

    SL_FreeMemory(json);
    cJSON_Delete(status);
}

void SL_AppTaskDevice(void *pData) {
    SL_EVENT ev = {0};
    SL_TASK stSltask;
    U32 lockState = LOCK_UNLOCKED;
    U32 lockUnlockStep = 0;
    SL_GPIO_PIN_STATUS pinStatus;

    SL_ApiPrint("******* SL_AppTaskDevice *********\n");
    SL_Memset(&ev, 0, sizeof(SL_EVENT));
    stSltask.element[0] = g_SLAppDevice;

    SL_GpioSetDir(GPIO_SWITCH_1, SL_GPIO_IN);
    SL_GpioSetDir(GPIO_SWITCH_2, SL_GPIO_IN);
//    SL_GpioSetDir(GPIO_BUZZER, SL_GPIO_OUT);
    SL_GpioSetDir(GPIO_MOTOR, SL_GPIO_OUT);
    SL_GpioWrite(GPIO_MOTOR, SL_PIN_LOW);

    if (SL_GpioRead(GPIO_SWITCH_1) == SL_PIN_HIGH) {
        lockState = LOCK_UNLOCKED;
        SL_StartTimer(stSltask, CHECK_SWITCH_1_TIME_ID, SL_TIMER_MODE_PERIODIC, SL_SecondToTicks(1));
    } else {
        lockState = LOCK_LOCKED;
    }

    while (1) {
        SL_FreeMemory((VOID *) ev.nParam1);
        SL_GetEvent(stSltask, &ev);

//        SL_ApiPrint("SLAPP: SL_AppTaskDevice get event[%d]\n", ev.nEventId);
        switch (ev.nEventId) {
            case EVT_APP_UNLOCK:
                SL_ApiPrint("SL_AppTaskDevice: EVT_APP_UNLOCK");
                if (lockState != LOCK_LOCKED) {
                    SL_ApiPrint("lock state: %d", lockState);
                    break;
                }
                lockState = LOCK_UNLOCKING;
                lockUnlockStep = 0;
                SL_StartTimer(stSltask, CHECK_SWITCH_2_TIME_ID, SL_TIMER_MODE_PERIODIC, SL_MilliSecondToTicks(100));
                /* start motor */
                SL_GpioWrite(GPIO_MOTOR, SL_PIN_HIGH);
                break;
            case EVT_APP_REPORT_STATUS:
                SL_ApiPrint("SL_AppTaskDevice: EVT_APP_REPORT_STATUS");
                SL_AppReportStatus(lockState);
                break;
            case SL_EV_TIMER:
                if (ev.nParam1 == CHECK_SWITCH_2_TIME_ID) {
                    SL_ApiPrint("CHECK_SWITCH_2_TIME_ID");
                    pinStatus = SL_GpioRead(GPIO_SWITCH_2);
                    if (pinStatus == SL_PIN_LOW) {
                        if (lockUnlockStep == 0) {
                            lockUnlockStep = 1;
                        } else if (lockUnlockStep == 2) {
                            /* stop motor */
                            SL_GpioWrite(GPIO_MOTOR, SL_PIN_LOW);
                            SL_StopTimer(stSltask, CHECK_SWITCH_2_TIME_ID);
                            if (lockState == LOCK_LOCKING) {
                                lockState = LOCK_LOCKED;
                                SL_ApiPrint("lock finished");
                            } else {
                                lockState = LOCK_UNLOCKED;
                                SL_ApiPrint("unlock finished");
                                SL_StartTimer(stSltask, CHECK_SWITCH_1_TIME_ID, SL_TIMER_MODE_PERIODIC,
                                              SL_SecondToTicks(1));
                            }
                            SL_AppSendMsg(g_SLAppDevice, EVT_APP_REPORT_STATUS, 0);
                        }
                    } else {
                        if (lockUnlockStep == 1) {
                            lockUnlockStep = 2;
                        }
                    }
                } else if (ev.nParam1 == CHECK_SWITCH_1_TIME_ID) {
                    SL_ApiPrint("CHECK_SWITCH_1_TIME_ID");
                    pinStatus = SL_GpioRead(GPIO_SWITCH_1);
                    if (pinStatus == SL_PIN_LOW) {
                        SL_StopTimer(stSltask, CHECK_SWITCH_1_TIME_ID);
                        lockState = LOCK_LOCKING;
                        lockUnlockStep = 0;
                        SL_StartTimer(stSltask, CHECK_SWITCH_2_TIME_ID, SL_TIMER_MODE_PERIODIC,
                                      SL_MilliSecondToTicks(20));
                        /* start motor */
                        SL_GpioWrite(GPIO_MOTOR, SL_PIN_HIGH);
                    }
                }
                break;
            default:
                break;
        }
    }
}

void SL_AppTaskYunba(void *pData) {
    SL_EVENT ev = {0};
    SL_TASK stSltask;
    U32 mqttOk = 0;

    SL_ApiPrint("******* SL_AppTaskYunba *********\n");
    SL_Memset(&ev, 0, sizeof(SL_EVENT));
    stSltask.element[0] = g_SLAppYunba;;
    SL_AppSendMsg(g_SLAppYunba, EVT_APP_READY, 0);

    while (1) {
        SL_FreeMemory((VOID *) ev.nParam1);
        SL_GetEvent(stSltask, &ev);

        SL_ApiPrint("SLAPP: SL_AppTaskYunba get event[%d]\n", ev.nEventId);
        switch (ev.nEventId) {
            case EVT_APP_READY:
                SL_ApiPrint("SL_AppTaskYunba: EVT_APP_READY");
                while (SL_GetNwStatus() != SL_RET_OK) {
                    SL_ApiPrint("SLAPP: network not ok");
                    SL_Sleep(1000);
                }
                SL_ApiPrint("SLAPP: network ok");
                MQTTInit(g_SLAppYunba);
                break;
            case EVT_APP_MQTT_ERROR:
                SL_ApiPrint("SL_AppTaskYunba: EVT_APP_MQTT_ERROR");
//                SL_Reset();
                mqttOk = 0;
                break;
            case EVT_APP_MQTT_INIT_OK:
                SL_ApiPrint("SL_AppTaskYunba: EVT_APP_MQTT_INIT_OK");
                MQTTConnect();
                break;
            case EVT_APP_MQTT_CONNACK:
                SL_ApiPrint("SL_AppTaskYunba: EVT_APP_MQTT_CONNACK");
                mqttOk = 1;
                MQTTSetAlias(ALIAS);
                SL_StartTimer(stSltask, MQTT_KEEPALIVE_TIME_ID, SL_TIMER_MODE_PERIODIC, SL_SecondToTicks(60));
                SL_AppSendMsg(g_SLAppDevice, EVT_APP_REPORT_STATUS, 0);
                break;
            case EVT_APP_MQTT_PUBLISH:
                SL_ApiPrint("SL_AppTaskYunba: EVT_APP_MQTT_PUBLISH");
                SL_ApiPrint("payload: %s", ev.nParam1);
                SL_AppHandleYunbaMsg(ev.nParam1);
                break;
            case EVT_APP_MQTT_EXTCMD:
                SL_ApiPrint("SL_AppTaskYunba: EVT_APP_MQTT_EXCMD");
                SL_ApiPrint("payload: %s", ev.nParam1);
                break;
            case SL_EV_TIMER:
                if (ev.nParam1 == MQTT_KEEPALIVE_TIME_ID) {
                    SL_ApiPrint("SL_AppTaskYunba: MQTT_KEEPALIVE_TIME_ID");
                    MQTTPingreq();
                }
                break;
            default:
                break;
        }
    }
}

void SL_AppCreateTask() {
    g_SLAppDevice = SL_CreateTask(SL_AppTaskDevice, APP_TASK_DEVICE_STACK_SIZE, APP_TASK_DEVICE_PRIORITY,
                                  "SL_AppTaskDevice");
    g_SLAppYunba = SL_CreateTask(SL_AppTaskYunba, APP_TASK_YUNBA_STACK_SIZE, APP_TASK_YUNBA_PRIORITY,
                                 "SL_AppTaskYunba");
}

void APP_ENTRY_START

SL_Entry(void) {
    SL_EVENT ev = {0};
    SL_TASK stSltask;
    PSL_UART_DATA pUartData;
    U32 ulUartId;

    SL_Memset(&ev, 0, sizeof(SL_EVENT));
    SL_AppCreateTask();

    stSltask.element[0] = SL_GetAppTaskHandle();
    SL_AppSendMsg(stSltask.element[0], EVT_APP_READY, 0);

    while (1) {
        SL_FreeMemory((VOID *) ev.nParam1);
        SL_GetEvent(stSltask, &ev);
        SL_ApiPrint("SLAPP: SL_Entry get event[%d]\n", ev.nEventId);
        switch (ev.nEventId) {
            case EVT_APP_READY:
                // SL_AppInitTcpip();

//                while (SL_GetNwStatus() != SL_RET_OK) {
//                    SL_ApiPrint("SLAPP: net register");
//                    SL_Sleep(1000);
//                }

//                SL_ApiPrint("SLAPP: net register ok");
//                SL_Sleep(20000);
//                SL_ApiPrint("SLAPP: test unlock");
//                SL_AppSendMsg(g_SLAppDevice, EVT_APP_UNLOCK, 0);
                //SL_ApiPrint("SLAPP: open uart2");
                //SL_UartOpen(SL_UART_2);
                //SL_UartSetBaudRate(SL_UART_2, SL_UART_BAUD_RATE_115200);
                //SL_ApiPrint("SLAPP: open uart2 ok");

                //SL_UartOpen(SL_UART_1);
                //SL_UartSetBaudRate(SL_UART_1, SL_UART_BAUD_RATE_115200);
                //SL_UartSendData(SL_UART_1, "01234567890123456789012345678901234567890123456789", 50);
                //SL_UartClose(SL_UART_1);

                //SL_Sleep(30 * 1000);
                //SL_AppFtpStartTcpip();

                // wifi_init();
                // wifi_power_on();

                //SL_LpwrEnterDSleep(SL_UART_1);
                //SL_UartClose(SL_UART_1);
                //SL_UartClose(SL_UART_2);
                //SL_AppStartTcpip();
                break;
//            case EVT_APP_GPRS_READY:
//                SL_AppStartTcpip();
//                break;
//            case EVT_APP_TCP_CLOSE:
//                SL_ApiPrint("SLAPP: resv EVT_APP_TCP_CLOSE socket[%d]", ev.nParam1);
//                SL_TcpipSocketClose(ev.nParam1);
//                break;
            case SL_EV_UART_RECEIVE_DATA_IND:
                pUartData = (PSL_UART_DATA) ev.nParam1;
                ulUartId = ev.nParam2;

                //gucTaskId = SL_TakeMutex(gucMutex);
                //SL_Memset(gucUartRecvBuff, 0, sizeof(gucUartRecvBuff));
                if (gucUartRecvBuffCurPosi + pUartData->ulDataLen > sizeof(gucUartRecvBuff)) {
                    SL_Memset(gucUartRecvBuff, 0, sizeof(gucUartRecvBuff));
                    gucUartRecvBuffCurPosi = 0;
                }

                SL_Memcpy(gucUartRecvBuff + gucUartRecvBuffCurPosi, pUartData->aucDataBuf, pUartData->ulDataLen);
                SL_ApiPrint("SLAPP: uart[%d] read uart lenth[%d]", ulUartId, pUartData->ulDataLen);
                gucUartRecvBuffCurPosi += pUartData->ulDataLen;

                //SL_GiveMutex(gucMutex, gucTaskId);
                break;
            case SL_EV_TIMER:
                break;
            case SL_EV_UART1_WKUP_IRQ_IND:
                SL_Print("****** SL_EV_UART1_WKUP_IRQ_IND ******\n");
                SL_LpwrEnterWakeup();
                SL_UartOpen(SL_UART_1);
                break;

            default:
                break;
        }
    }
}
