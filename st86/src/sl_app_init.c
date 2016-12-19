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
#include "sl_app_agps.h"
#include "sl_app_event.h"
#include "cJSON.h"
#include "sl_app_mqttclient.h"

#define ALIAS "yunba_lock_test"
#define REPORT_TOPIC "yunba_lock_report"

#define APP_TASK_DEVICE_STACK_SIZE    (3 * 2048)
#define APP_TASK_DEVICE_PRIORITY      (SL_APP_TASK_PRIORITY_BEGIN + 1)
#define APP_TASK_YUNBA_STACK_SIZE     (3 * 2048)
#define APP_TASK_YUNBA_PRIORITY       (SL_APP_TASK_PRIORITY_BEGIN + 2)

#define GPIO_SWITCH_1   SL_GPIO_1
#define GPIO_SWITCH_2   SL_GPIO_2
#define GPIO_BUZZER     SL_GPIO_5
#define GPIO_MOTOR      SL_GPIO_6

#define TEST_TIME_ID 200
#define CHECK_SWITCH_1_TIME_ID 201
#define CHECK_SWITCH_2_TIME_ID 202
#define MQTT_KEEPALIVE_TIME_ID 203
#define GET_AGPS_TIME_ID 204
#define BUZZER_OFF_TIME_ID 205

typedef enum {
    LOCK_UNLOCKED,
    LOCK_LOCKED,
    LOCK_UNLOCKING,
    LOCK_LOCKING
} LOCK_STATUS;

/* variable */
static HANDLE gSLAppDevice = HNULL;
static HANDLE gSLAppYunba = HNULL;

static U32 gLongitude;
static U32 gLatitude;

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
    char *cmd;
    if (root) {
        int ret_size = cJSON_GetArraySize(root);
        if (ret_size >= 1) {
            cmd = cJSON_GetObjectItem(root, "cmd")->valuestring;
            if (strcmp(cmd, "unlock") == 0) {
                SL_AppSendMsg(gSLAppDevice, EVT_APP_UNLOCK, 0);
            } else if (strcmp(cmd, "report") == 0) {
                SL_AppSendMsg(gSLAppDevice, EVT_APP_REPORT_STATUS, 0);
            } else if (strcmp(cmd, "buzzer_on") == 0) {
                SL_AppSendMsg(gSLAppDevice, EVT_APP_BUZZER_ON, 5);
            } else if (strcmp(cmd, "buzzer_off") == 0) {
                SL_AppSendMsg(gSLAppDevice, EVT_APP_BUZZER_OFF, 0);
            }
        }
        cJSON_Delete(root);
    }
}

void SL_AppReportStatus(S32 lockState) {
    cJSON *status;
    char *json;
    char buf[64];

    status = cJSON_CreateObject();
    cJSON_AddStringToObject(status, "alias", ALIAS);
    cJSON_AddBoolToObject(status, "lock", lockState == LOCK_LOCKED || lockState == LOCK_LOCKING);
    snprintf(buf, sizeof(buf), "%d", gLongitude);
    cJSON_AddStringToObject(status, "longitude", buf);
    snprintf(buf, sizeof(buf), "%d", gLatitude);
    cJSON_AddStringToObject(status, "latitude", buf);

    json = cJSON_PrintUnformatted(status);
    //SL_ApiPrint("status: %s", json);

    MQTTPublish(REPORT_TOPIC, json);

    SL_FreeMemory(json);
    cJSON_Delete(status);
}

void SL_AppAssistGpsGetLocCb(S32 slResult, U32 ulLonggitude, U32 ulLatidude) {
    if ((0 == ulLonggitude) || (0 == ulLatidude)) {
        SL_Print("AGPS loc fail this time for lat/lon is 0\n");
        return;
    }
    //SL_ApiPrint("SLAPP: SL_AppAssistGpsGetLocCb result[%d], longi[%d], lati[%d]",
//                slResult, ulLonggitude, ulLatidude);
    gLongitude = ulLonggitude;
    gLatitude = ulLatidude;
    SL_AppSendMsg(gSLAppDevice, EVT_APP_REPORT_STATUS, 0);
}

void SL_AppTaskDevice(void *pData) {
    SL_EVENT ev = {0};
    SL_TASK stSltask;
    U32 lockState = LOCK_UNLOCKED;
    U32 lockUnlockStep = 0;
    SL_GPIO_PIN_STATUS pinStatus;

    //SL_ApiPrint("******* SL_AppTaskDevice *********\n");
    SL_Memset(&ev, 0, sizeof(SL_EVENT));
    stSltask.element[0] = gSLAppDevice;

    SL_GpioSetDir(GPIO_SWITCH_1, SL_GPIO_IN);
    SL_GpioSetDir(GPIO_SWITCH_2, SL_GPIO_IN);
//    SL_GpioSetDir(GPIO_BUZZER, SL_GPIO_OUT);
//    SL_GpioWrite(GPIO_BUZZER, SL_PIN_HIGH);
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

//        //SL_ApiPrint("SLAPP: SL_AppTaskDevice get event[%d]\n", ev.nEventId);
        switch (ev.nEventId) {
            case EVT_APP_UNLOCK:
                //SL_ApiPrint("SL_AppTaskDevice: EVT_APP_UNLOCK");
                if (lockState != LOCK_LOCKED) {
                    //SL_ApiPrint("lock state: %d", lockState);
                    break;
                }
                lockState = LOCK_UNLOCKING;
                lockUnlockStep = 0;
                SL_StartTimer(stSltask, CHECK_SWITCH_2_TIME_ID, SL_TIMER_MODE_PERIODIC, SL_MilliSecondToTicks(100));
                /* start motor */
                SL_GpioWrite(GPIO_MOTOR, SL_PIN_HIGH);
                break;
            case EVT_APP_REPORT_STATUS:
                //SL_ApiPrint("SL_AppTaskDevice: EVT_APP_REPORT_STATUS");
                SL_AppReportStatus(lockState);
                break;
//            case EVT_APP_BUZZER_ON:
//                //SL_ApiPrint("SL_AppTaskDevice: EVT_APP_BUZZER_ON");
//                SL_GpioWrite(GPIO_BUZZER, SL_PIN_LOW);
//                SL_StartTimer(stSltask, BUZZER_OFF_TIME_ID, SL_TIMER_MODE_SINGLE, SL_SecondToTicks(ev.nParam1));
//                break;
//            case EVT_APP_BUZZER_OFF:
//                //SL_ApiPrint("SL_AppTaskDevice: EVT_APP_BUZZER_OFF");
//                SL_GpioWrite(GPIO_BUZZER, SL_PIN_HIGH);
//                break;
            case SL_EV_TIMER:
                if (ev.nParam1 == CHECK_SWITCH_2_TIME_ID) {
                    //SL_ApiPrint("CHECK_SWITCH_2_TIME_ID");
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
                                //SL_ApiPrint("lock finished");
                            } else {
                                lockState = LOCK_UNLOCKED;
                                //SL_ApiPrint("unlock finished");
                                SL_StartTimer(stSltask, CHECK_SWITCH_1_TIME_ID, SL_TIMER_MODE_PERIODIC,
                                              SL_MilliSecondToTicks(400));
                            }
                        }
                    } else {
                        if (lockUnlockStep == 1) {
                            lockUnlockStep = 2;
                            SL_AppSendMsg(gSLAppDevice, EVT_APP_REPORT_STATUS, 0);
                        }
                    }
                } else if (ev.nParam1 == CHECK_SWITCH_1_TIME_ID) {
                    //SL_ApiPrint("CHECK_SWITCH_1_TIME_ID");
                    pinStatus = SL_GpioRead(GPIO_SWITCH_1);
                    if (pinStatus == SL_PIN_LOW) {
                        SL_StopTimer(stSltask, CHECK_SWITCH_1_TIME_ID);
                        lockState = LOCK_LOCKING;
                        lockUnlockStep = 0;
                        SL_StartTimer(stSltask, CHECK_SWITCH_2_TIME_ID, SL_TIMER_MODE_PERIODIC,
                                      SL_MilliSecondToTicks(100));
                        /* start motor */
                        SL_GpioWrite(GPIO_MOTOR, SL_PIN_HIGH);
                    }
                } else if (ev.nParam1 == BUZZER_OFF_TIME_ID) {
//                    //SL_ApiPrint("BUZZER_OFF_TIME_ID");
//                    SL_GpioWrite(GPIO_BUZZER, SL_PIN_HIGH);
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

    //SL_ApiPrint("******* SL_AppTaskYunba *********\n");
    SL_Memset(&ev, 0, sizeof(SL_EVENT));
    stSltask.element[0] = gSLAppYunba;;
    SL_AppSendMsg(gSLAppYunba, EVT_APP_READY, 0);

    while (1) {
        SL_FreeMemory((VOID *) ev.nParam1);
        SL_GetEvent(stSltask, &ev);

        //SL_ApiPrint("SLAPP: SL_AppTaskYunba get event[%d]\n", ev.nEventId);
        switch (ev.nEventId) {
            case EVT_APP_READY:
                //SL_ApiPrint("SL_AppTaskYunba: EVT_APP_READY");
                while (SL_GetNwStatus() != SL_RET_OK) {
                    //SL_ApiPrint("SLAPP: network not ok");
                    SL_Sleep(1000);
                }
                //SL_ApiPrint("SLAPP: network ok");
//                SL_AppInitAgps();
                MQTTInit(gSLAppYunba);
//                SL_StartTimer(stSltask, GET_AGPS_TIME_ID, SL_TIMER_MODE_PERIODIC, SL_SecondToTicks(20));
                break;
            case EVT_APP_MQTT_ERROR:
                //SL_ApiPrint("SL_AppTaskYunba: EVT_APP_MQTT_ERROR");
                SL_Reset();
                mqttOk = 0;
                break;
            case EVT_APP_MQTT_INIT_OK:
                //SL_ApiPrint("SL_AppTaskYunba: EVT_APP_MQTT_INIT_OK");
                MQTTConnect();
                break;
            case EVT_APP_MQTT_CONNACK:
                //SL_ApiPrint("SL_AppTaskYunba: EVT_APP_MQTT_CONNACK");
                mqttOk = 1;
                MQTTSetAlias(ALIAS);
                SL_StartTimer(stSltask, MQTT_KEEPALIVE_TIME_ID, SL_TIMER_MODE_PERIODIC, SL_SecondToTicks(60));
//                SL_StartTimer(stSltask, GET_AGPS_TIME_ID, SL_TIMER_MODE_PERIODIC, SL_SecondToTicks(20));
                SL_AppSendMsg(gSLAppDevice, EVT_APP_REPORT_STATUS, 0);
                break;
            case EVT_APP_MQTT_PUBLISH:
                //SL_ApiPrint("SL_AppTaskYunba: EVT_APP_MQTT_PUBLISH");
                //SL_ApiPrint("payload: %s", ev.nParam1);
                SL_AppHandleYunbaMsg(ev.nParam1);
                break;
            case EVT_APP_MQTT_EXTCMD:
                //SL_ApiPrint("SL_AppTaskYunba: EVT_APP_MQTT_EXCMD");
                //SL_ApiPrint("payload: %s", ev.nParam1);
                break;
            case SL_EV_TIMER:
                if (ev.nParam1 == MQTT_KEEPALIVE_TIME_ID) {
                    //SL_ApiPrint("SL_AppTaskYunba: MQTT_KEEPALIVE_TIME_ID");
                    MQTTPingreq();
                } else if (ev.nParam1 == GET_AGPS_TIME_ID) {
                    //SL_ApiPrint("SL_AppTaskYunba: GET_AGPS_TIME_ID");
                    SL_AssistGpsGetLoc(SL_AppAssistGpsGetLocCb);
                }
                break;
            default:
                break;
        }
    }
}

void SL_AppCreateTask() {
    gSLAppDevice = SL_CreateTask(SL_AppTaskDevice, APP_TASK_DEVICE_STACK_SIZE, APP_TASK_DEVICE_PRIORITY,
                                 "SL_AppTaskDevice");
    gSLAppYunba = SL_CreateTask(SL_AppTaskYunba, APP_TASK_YUNBA_STACK_SIZE, APP_TASK_YUNBA_PRIORITY,
                                "SL_AppTaskYunba");
}

void SL_AppInitAgps(void) {
    S32 slRet = 0;
    U8 attach_st = 0;
    U8 active_st = 0;

    slRet = SL_GprsGetAttState(&attach_st);
    if (slRet != SL_RET_OK) {
        SL_Print("AGPS get gprs attach stat error, exit!\n");
        return;
    }

    if (SL_GPRS_ATTACHED == attach_st) {
        SL_Print("AGPS get gprs attach state attached, check active gprs state first\n");
        slRet = SL_GprsGetActState(1, &active_st);
        if (slRet != SL_RET_OK) {
            SL_Print("AGPS get gprs active stat error, exit!\n");
            return;
        }

        if (SL_GPRS_ACTIVED == active_st) {
            SL_Print("AGPS gprs already actived, start agps location...\n");
            slRet = SL_AssistGpsGetLoc(SL_AppAssistGpsGetLocCb);
            if (slRet != SL_RET_OK) {
                SL_Print("AGPS get location error for no attached/actived gprs, exit\n");
                return;
            }
        }
    }
}

void APP_ENTRY_START

SL_Entry(void) {
    SL_EVENT ev = {0};
    SL_TASK stSltask;

    SL_Memset(&ev, 0, sizeof(SL_EVENT));
    SL_AppCreateTask();

    stSltask.element[0] = SL_GetAppTaskHandle();
    SL_AppSendMsg(stSltask.element[0], EVT_APP_READY, 0);

    while (1) {
        SL_FreeMemory((VOID *) ev.nParam1);
        SL_GetEvent(stSltask, &ev);
        //SL_ApiPrint("SLAPP: SL_Entry get event[%d]\n", ev.nEventId);
        switch (ev.nEventId) {
            case EVT_APP_READY:
//                while (SL_GetNwStatus() != SL_RET_OK) {
//                    //SL_ApiPrint("SLAPP: net register");
//                    SL_Sleep(1000);
//                }
//
//                SL_AssistGpsConfig("52.57.19.50", 7275);
//                SL_AppStartAgps();
                break;
            default:
                break;
        }
    }
}
