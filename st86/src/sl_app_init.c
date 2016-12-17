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
#include "sl_audio.h"
#include "sl_gpio.h"
#include "sl_lowpower.h"
#include "sl_filesystem.h"
#include "sl_assistgps.h"
#include "sl_app_sms.h"
#include "sl_app_call.h"
#include "sl_app_event.h"
#include "sl_app_tcpip.h"
#include "sl_app_audio.h"
#include "sl_app_agps.h"
#include "sl_app_audiorec.h"
#include "sl_app_dtmf.h"
#include "sl_app_tts.h"
#include "sl_app_filesystem.h"
#include "sl_app_pbk.h"
#include "sl_app_sim.h"
#include "sl_app_ftp.h"
#include "sl_app_bt.h"
#include "sl_app_wifi.h"

//GPS test
//#define TEST_GPS
#ifdef TEST_GPS
#define GPS_TIME_ID  200
#endif



/*
 * define for test, update the items for each test
*/
/* 
 * For case
 */

/* timer */ 
#define APP_TIMER_AUDIO_REC       3

#define APP_TASK1_STACK_SIZE    (2 * 2048)
#define APP_TASK1_PRIORITY      (SL_APP_TASK_PRIORITY_BEGIN + 1)

#define APP_TASK2_STACK_SIZE    (2 * 2048)
#define APP_TASK2_PRIORITY      (APP_TASK1_PRIORITY + 1)

#define APP_TASK3_STACK_SIZE    (2 * 2048)
#define APP_TASK3_PRIORITY      (APP_TASK2_PRIORITY + 1)

#define APP_TASK4_STACK_SIZE    (2 * 2048)
#define APP_TASK4_PRIORITY      (APP_TASK3_PRIORITY + 1)

#define LIB_VER_LEN             20
#define APP_VER_LEN             20
#define IMEI_LEN                20
#define SN_LEN                  20
#define IMSI_LEN                20
#define ICCID_LEN               20

/* variable */ 
HANDLE g_SLApp1 = HNULL;
HANDLE g_SLApp2 = HNULL;
HANDLE g_SLApp3 = HNULL;
HANDLE g_SLAppFtpTask = HNULL;

static U8 gucUartRecvBuff[100] = {0};
static U8 gucUartRecvBuffCurPosi = 0;
SL_JMP_BUF jb;
U8 gucMutex = 0;
U8 gucTaskId = 0;

U8 g_LibVer[LIB_VER_LEN] = {0};
U8 g_AppVer[APP_VER_LEN] = {0};
U8 g_IMEI[IMEI_LEN] = {0};
U8 g_SN [SN_LEN] = {0};
U8 g_IMSI[IMSI_LEN] = {0};
U8 g_ICCID[ICCID_LEN] = {0};


/****************************************************
                                  function

****************************************************/
void SL_AppPrint(DOUBLE w1, DOUBLE w2)
{
    S32 slValue = 0;
    //double w1 = 1.0, w2 = 2.0;

    slValue = 100 * SL_Sin(w1);
    SL_ApiPrint("SLAPP: SL_AppPrint sin[%d]", slValue);
    slValue = 1000 * SL_Cos(w2);
    SL_ApiPrint("SLAPP: SL_AppPrint cos[%d]", slValue);
    slValue = 1000 * SL_Acos(0.5);
    SL_ApiPrint("SLAPP: SL_AppPrint arcos[%d]", slValue);
    
    slValue = 1000 * SL_Tan(0.5);
    SL_ApiPrint("SLAPP: SL_AppPrint tan[%d]", slValue);
    slValue = 1000 * SL_Atan(4);
    SL_ApiPrint("SLAPP: SL_AppPrint arctan[%d]", slValue);

    slValue = 1000 * SL_Sqrt(9);
    SL_ApiPrint("SLAPP: SL_AppPrint sqrt[%d]", slValue);
}

void SL_AppInitGpio()
{
    SL_GPIO_IRQ_MASK stGpioMask;

    SL_Memset(&stGpioMask, 0, sizeof(SL_GPIO_IRQ_MASK));
    stGpioMask.debounce = TRUE;
    stGpioMask.falling = FALSE;
    stGpioMask.level = 0;
    stGpioMask.rising = TRUE;
    SL_GpioIntDetectInit(1, &stGpioMask, NULL);
}
void raise_exception(void)
{
	SL_ApiPrint("testjmp:exception raised\n");
	SL_LongJmp(jb, 1); /* jump to exception handler */
	SL_ApiPrint("testjmp:this line1 should never appear\n");
} 

int SL_AppTestJmp(void)
{
	if(SL_SetJmp(jb) == 0)
	{
		SL_ApiPrint("testjmp:\''setjmp\'' is initializing \''j\''\n");
		raise_exception();
		SL_ApiPrint("testjmp:this line2 should never appear\n");
	}
	else
	{
		SL_ApiPrint("testjmp:''setjmp'' was just jumped into\n");
		/* this code is the exception handler */
	}
	return 0;
} 

void SL_AppGetCellInfo(S32 slResult, SL_NW_CELL_INFO* pstSlCellInfo)
{
    SL_ApiPrint("SLAPP: SL_AppGetCellInfo result[%d]", slResult);
    SL_ApiPrint("cell num[%d]", pstSlCellInfo->ucCellNum);
    for(U8 i = 0; i < pstSlCellInfo->ucCellNum; i++)
    {
        SL_ApiPrint("cell mcc[%d], mnc[%d], lac[%d], ci[%d], bsic[%d], rxlev[%d], ta[%d], freq[%d]", 
            pstSlCellInfo->stSlCellInfo[i].ulSlMCC, 
            pstSlCellInfo->stSlCellInfo[i].ulSlMNC,pstSlCellInfo->stSlCellInfo[i].ulSlLAC,
            pstSlCellInfo->stSlCellInfo[i].ulSlCI, pstSlCellInfo->stSlCellInfo[i].iBsic,
            pstSlCellInfo->stSlCellInfo[i].iRxLev, pstSlCellInfo->stSlCellInfo[i].ucTimeAdv,
            pstSlCellInfo->stSlCellInfo[i].usFreq);
    }
}

void SL_AppSendMsg(HANDLE stTask, U32 ulMsgId, U32 ulParam)
{
    SL_EVENT stEvnet;
    SL_TASK hTask;

    SL_Memset(&stEvnet, 0, sizeof(SL_EVENT));
    stEvnet.nEventId = ulMsgId;
    stEvnet.nParam1 = ulParam;
    hTask.element[0] = stTask;
    SL_SendEvents(hTask, &stEvnet);
}

void SL_AppTask1(void *pData)
{
    SL_EVENT ev = {0};
    SL_TASK stSltask;
    PSL_UART_DATA pUartData;
    U32 ulUartId;
    
    SL_ApiPrint("******* SL_AppTask1 *********\n");
    SL_Memset(&ev, 0, sizeof(SL_EVENT));
    stSltask.element[0] = g_SLApp1;
    
    while(1)
    {
        SL_FreeMemory((VOID*)ev.nParam1);
        SL_GetEvent(stSltask, &ev);

        SL_ApiPrint("SLAPP: SL_AppTask1 get event[%d]\n", ev.nEventId);
        switch (ev.nEventId)
        {
            case EVT_APP_GPRS_STARTRECV:
            {
                
            }
            break;
            case EVT_APP_TCP_CLOSE:
            {
                SL_ApiPrint("SLAPP: resv EVT_APP_TCP_CLOSE socket[%d]", ev.nParam1);
                SL_TcpipSocketClose(ev.nParam1);
            }
            break;
            case SL_EV_TIMER:
            {
                SL_ApiPrint("SLAPP: recv timer[%d]", ev.nParam1);
                if(ev.nParam1 == APP_TIMER_AUDIO_REC)
                {
                    SL_AppAudioRecStop();
                }
            }
            break;
            default:
            break;
        }
    }
}

void SL_AppTask2(void *pData)
{
    SL_EVENT ev = {0};
    SL_TASK stSltask;

    SL_ApiPrint("******* SL_AppTask2 *********\n");
    SL_Memset(&ev, 0, sizeof(SL_EVENT));
    stSltask.element[0] = g_SLApp2;
    while(1)
    {
        SL_FreeMemory((VOID*)ev.nParam1);
        SL_GetEvent(stSltask, &ev);

        SL_ApiPrint("SLPP: SL_AppTask2 get event[%d]\n", ev.nEventId);
        switch (ev.nEventId)
        {
            case EVT_APP_GPRS_READY:
            {
                //SL_AppTcpipConn(2, SL_TCPIP_SOCK_TCP, aucServerIp, usServerPort + 2);
            }
            break;
            case EVT_APP_GPRS_STARTRECV:
            {
                
            }
            break;
            case EVT_APP_TCP_CLOSE:
            {
                SL_ApiPrint("SLAPP: recv EVT_APP_TCP_CLOSE socket[%d]", ev.nParam1);
                //SL_TcpipSocketClose(ev.nParam1);
            }
            break;

            case EVT_APP_AGPS:
            {
                SL_AppStartAgps();
            }
            break;

            case EVT_APP_DTMF:
            {
                SL_AppStartDtmf();
            }
            case SL_EV_TIMER:
            {
            }
            break;
            default:
            break;
        }
    }
}

void SL_AppTask3(void *pData)
{
    SL_EVENT ev = {0};
    SL_TASK stSltask;
    
    SL_ApiPrint("******* SL_AppTask3 *********\n");
    
    SL_Memset(&ev, 0, sizeof(SL_EVENT));
    stSltask.element[0] = g_SLApp3;
    
    while(1)
    {
        SL_FreeMemory((VOID*)ev.nParam1);
        SL_GetEvent(stSltask, &ev);

        SL_ApiPrint("SLPP: SL_AppTask3 get event[%d]\n", ev.nEventId);
        switch (ev.nEventId)
        {
            case EVT_APP_CREATE_FILE:
            {
                SL_AppFileCreateHandle();
            }
            break;
            
            case EVT_APP_READ_FILE:
            {
                SL_AppFileReadHandle();
            }
            break;

            case EVT_APP_PLAY_FILE:
            {   
                SL_AppPlayAudio();
            }
            break;

            case EVT_APP_PLAY_TTS:
            {
                SL_AppPlayTts();
            }
            break;
            
            case SL_EV_TIMER:
            {
            }
            break;
            default:
            break;
        }
    }
}

void SL_AppTask4(void *pData)
{
    U32 cache_data_len = 0;
    U8* ftp_data;
    U32 app_up_st = 0;
    SL_EVENT ev = {0};
    SL_TASK stSltask;
    
    SL_ApiPrint("******* SL_AppTask4 *********\n");
    
    SL_Memset(&ev, 0, sizeof(SL_EVENT));
    stSltask.element[0] = g_SLAppFtpTask;
    
    while(1)
    {
        SL_FreeMemory((VOID*)ev.nParam1);
        SL_GetEvent(stSltask, &ev);

        SL_ApiPrint("SLPP: SL_AppTask4 get event[%d]\n", ev.nEventId);
        switch (ev.nEventId)
        {
            case SL_APP_FTPOP_REQ:
            {
                SL_AppFtpOpen();
            }
            break;
            
            case SL_APP_UPDATE_REQ:
            {
                cache_data_len = ev.nParam2;
                SL_ApiPrint("---> FTP get file len:%d\n", cache_data_len);
            
                if (cache_data_len < APP_CODE_MAXLEN) //app max 310K byte
                {
                    SL_ApiPrint("---> FTP get data in ram\n");
                    ftp_data = (U8 *)ev.nParam1;    
                    app_up_st = SL_AppUpdateInit(ftp_data, cache_data_len);
                    SL_ApiPrint("---> SL APP update init ret:%d\n", app_up_st);
                    if (SL_APP_UPDATE_OK != app_up_st)
                    {
                        SL_ApiPrint("---> SL App update condition check fail, do not update!\n");
                    }
                    else
                    {
                        SL_ApiPrint("---> SL App update condition check ok, reset system and update ... \n");
                        SL_PowerDown(0); //ÖØÆô
                    }
                }
                else
                    SL_ApiPrint("---> FTP get file size overflow:%d, do not download!\n", cache_data_len);
            }
            break;
            
            case SL_EV_TIMER:
            {
            }
            break;
            default:
            break;
        }
    }
}

void SL_AppCreateTask()
{
   g_SLApp1 = SL_CreateTask(SL_AppTask1, APP_TASK1_STACK_SIZE, APP_TASK1_PRIORITY, "SL_AppTask1");
   SL_ApiPrint("g_SLApp1=%u", g_SLApp1);
   
   g_SLApp2 = SL_CreateTask(SL_AppTask2, APP_TASK2_STACK_SIZE, APP_TASK2_PRIORITY, "SL_AppTask2");
   SL_ApiPrint("g_SLApp2=%u", g_SLApp2);

   g_SLApp3 = SL_CreateTask(SL_AppTask3, APP_TASK3_STACK_SIZE, APP_TASK3_PRIORITY, "SL_AppTask3");
   SL_ApiPrint("g_SLApp3=%u", g_SLApp3);
   
   g_SLAppFtpTask = SL_CreateTask(SL_AppTask4, APP_TASK3_STACK_SIZE, APP_TASK4_PRIORITY, "SL_AppTask4");
   SL_ApiPrint("g_SLApp4=%u", g_SLAppFtpTask);
}

/****************ENTRY FUNCTION*********************/
extern void SL_AppSmsTest();
extern void SL_AppFlashReadId(void);
extern INT32 SL_AppFlashReadData(UINT32 ulStartAddrss, UINT8* pucBuff, UINT32 ulSize);

void SL_AppEnterFlightMode(S32 slResult)
{
    SL_ApiPrint("SLAPP: enter flight mode,[%d]", slResult);
}

void SL_AppExitFlightMode(S32 slResult)
{
    SL_ApiPrint("SLAPP: exit flight mode, [%d]", slResult);
}

#ifdef TEST_GPS  
void Sl_gps_result(CHAR* p_gps)
{
    SL_ApiPrint("SLAPP: gps info:%c",p_gps);
    SL_UartSendData(SL_UART_2,p_gps,SL_Strlen(p_gps));
}
#endif

void SL_AppPwrOn()
{
    SL_PowerOn();
    SL_ApiPrint("SLAPP: pwr on");
}

void SL_AppPwrOff()
{
    SL_PowerDown(1);
    SL_ApiPrint("SLAPP: pwr off");
}

void APP_ENTRY_START SL_Entry(void)
{
    SL_EVENT ev = {0};
    SL_TASK stSltask;
    PSL_UART_DATA pUartData;
    U32 ulUartId;
    U8 Imsi[30] = {0};
    U8 Imei[40] = {0};
    U8 Oper[10] = {0};
    SL_SYSTEMTIME SystemTime,SystemTime1;
    s32 ret = 0;
    U8 ucData[200] = {0};
    U32 ulFreeSpace, UlTotalSpace;
    S32 slSimCard, slCreg, slCgreg;
    S8 scRssi;
    U8 ucBer;
    U16 usBat = 0;
    U32 tmep = 0;
    SL_NW_NODEB_INFO stNodeB;
    U8 i = 0;
    
    SL_Memset(&ev, 0, sizeof(SL_EVENT));
    //SL_InitPwrOnCb(SL_AppPwrOn);
    SL_AppCreateTask();
    //SL_UartSetAppTaskHandle(SL_UART_1, g_SLApp1);
    //SL_AppSimPinInit();
    //SL_UartSetDlMode();
    #if 0
    while(SL_CoreInitFinish() == FALSE)
    {
        SL_ApiPrint("SLAPP: core init");
        SL_Sleep(100);
    }
    #endif
    //SL_AppSimPinQuery();
    
    stSltask.element[0] = SL_GetAppTaskHandle();
    SL_AppSendMsg(stSltask.element[0], EVT_APP_READY, 0);

#ifdef TEST_GPS
    SL_GpsModeConfig(1);
    SL_ApiPrint("SLAPP: GPS timer start!");
    SL_StartTimer(stSltask, GPS_TIME_ID, SL_TIMER_MODE_PERIODIC, SL_SecondToTicks(10));
#endif

    while(1)
    {
        SL_FreeMemory((VOID*)ev.nParam1);
        SL_GetEvent(stSltask, &ev);
        SL_ApiPrint("SLAPP: SL_Entry get event[%d]\n", ev.nEventId);
        switch (ev.nEventId)
        {
            case EVT_APP_READY:
            { 
                //SL_Sleep(3000);
                SL_AppInitCall();
                SL_AppInitTcpip();
                //SL_AppInitSms();
                SL_AppAudioRecInit();
                SL_AppPbkInit();
                SL_AppInitGpio();
#if SL_BT_APP==1
                SL_AppBtInit();
#endif

                SL_AppFtpInit();

                //#if 0
                while(SL_GetNwStatus() != SL_RET_OK)
                {
                    SL_ApiPrint("SLAPP: net register");
                    SL_Sleep(1000);
                }
                //#endif
                
                SL_GSM_GetIMEI(g_IMEI, sizeof(g_IMEI));
                SL_GSM_GetSN(g_SN, sizeof(g_SN));
                SL_GetCoreVer(g_LibVer, sizeof(g_LibVer));
                //SL_SimGetCCID(g_ICCID, sizeof(g_ICCID));
                SL_SIM_GetIMSI(g_IMSI, sizeof(g_IMSI));

                SL_ApiPrint("IMEI: %s.", g_IMEI);
                SL_ApiPrint("SN: %s.", g_SN);
                SL_ApiPrint("Lib ver: %s.", g_LibVer);
                //SL_ApiPrint("ICCID: %s.", g_ICCID);
                SL_ApiPrint("IMSI: %s.", g_IMSI);

                #if 0
                while (60 >= i)
                {
                    SL_ApiPrint("SLAPP: net register");
                    SL_Sleep(1000);
                    i++;
                }
                #endif
                
                #if 1
                while(SL_GetNwStatus() != SL_RET_OK)
                {
                    SL_ApiPrint("SLAPP: net register");
                    SL_Sleep(10000);
                }
                #endif
                
                SL_ApiPrint("SLAPP: net register ok");
                SL_Sleep(3000);
                //SL_ApiPrint("SLAPP: open uart2");
                //SL_UartOpen(SL_UART_2);
                //SL_UartSetBaudRate(SL_UART_2, SL_UART_BAUD_RATE_115200);
                //SL_ApiPrint("SLAPP: open uart2 ok");

                UINT32 totalSize = 0;
                UINT32 canUseSize = 0;
                UINT8 *pTempMem = NULL;
                totalSize = SL_MemTotalLeftSize();
                canUseSize = SL_MemMaxCanAllocSize();

                pTempMem = SL_GetMemory(320*1024);
                SL_Print("memory melloc :0x%08x, %d, %d.", pTempMem, totalSize, canUseSize);
                SL_Memset(pTempMem, 0, 320*1024);
                SL_FreeMemory(pTempMem);

                
                //SL_UartOpen(SL_UART_1);
                //SL_UartSetBaudRate(SL_UART_1, SL_UART_BAUD_RATE_115200);
                //SL_UartSendData(SL_UART_1, "01234567890123456789012345678901234567890123456789", 50);
                //SL_UartClose(SL_UART_1);

                //SL_Sleep(30 * 1000);
                //SL_AppFtpStartTcpip();

                wifi_init();
                wifi_power_on();
                
                #if 0
                while (1)
                {
                    SL_GetDeviceCurrentRunState(&slSimCard, &slCreg, &slCgreg, &scRssi, &ucBer);
                    SL_ApiPrint("sim:%d, creg:%d, cgreg:%d, rssi:%d, ber:%d.", slSimCard, slCreg, slCgreg, scRssi, ucBer);
                    SL_Sleep(1000);
                }
                #endif
                //SL_LpwrEnterDSleep(SL_UART_1);
                //SL_UartClose(SL_UART_1);
                //SL_UartClose(SL_UART_2);
                //SL_AppStartTcpip();

                //SL_WifiInit(SL_WIFI_CALLBACK * CB);

#if 0                

                //SL_RetrieveNodeBInfo(&stNodeB);
                //SL_Print("SLAPP: mcc[%d],mnc[%d],lac[%d],ci[%d]", stNodeB.ulSlMCC, stNodeB.ulSlMNC, stNodeB.ulSlLAC
                  //  ,stNodeB.ulSlCI);
                SL_InitPwrOffCb(SL_AppPwrOff);

                for(U8 i = 0; i < 20; i++)
                {
                    SL_RetrieveCellInfo(SL_NW_CELL_SERVING, SL_AppGetCellInfo);
                    SL_Print("SLAPP: get cur cell");
                    SL_Sleep(1000);
                    SL_RetrieveCellInfo(SL_NW_CELL_NEIGHBOR, SL_AppGetCellInfo);
                    SL_Print("SLAPP: get nei cell");
                    SL_Sleep(20000);
                }
#endif
#if 0
                ret = SL_GetRfTemp(&ucBer);
                SL_Print("SLAPP: pa temp(%u), ret[%d]", ucBer, ret);
                SL_AppSmsTest();
                
                stSltask.element[0] = SL_GetAppTaskHandle();
                SL_StartTimer(stSltask, APP_TIMER_CALL_HANGUP, SL_TIMER_MODE_PERIODIC, SL_SecondToTicks(300));
                //gucMutex = SL_CreateMutex();

                SL_FileSysGetSpaceInfo(0, &ulFreeSpace, &UlTotalSpace);
                SL_ApiPrint("SLAPP: ulFreeSpace[%d], total[%d]", ulFreeSpace, UlTotalSpace);    
                //SL_FileDelete("3");
                //SL_AppFileCreateHandle("6");
                SL_FileCreate("7");
                //SL_AppFileCreateHandle("4");
                SL_FileSysGetSpaceInfo(0, &ulFreeSpace, &UlTotalSpace);
                SL_ApiPrint("SLAPP: ulFreeSpace[%d], total[%d]", ulFreeSpace, UlTotalSpace);    

                SL_AppStartTcpip();
                while(0)
                {
                    SL_Sleep(10000);
                    SL_GetDeviceCurrentRunState(&slSimCard, &slCreg, &slCgreg, &scRssi, &ucBer);
                    SL_ApiPrint("SLAPP: rssi[%d], creg[%d], cgreg[%d]", scRssi, slCreg, slCgreg);
                }

                for(U8 i = 0; i < 20; i++)
                {
                    SL_RetrieveCellInfo(SL_NW_CELL_SERVING, SL_AppGetCellInfo);
                    SL_Sleep(5000);
                    SL_RetrieveCellInfo(SL_NW_CELL_NEIGHBOR, SL_AppGetCellInfo);
                    SL_Sleep(20000);
                }

                SL_PwlSetLevel(1, 32);
                SL_ApiPrint("SLAPP: PWM");
                SL_GetLocalTime(&SystemTime);
                SL_ApiPrint("SLAPP: year[%d], mon[%d], min[%d], hour[%d], dweek[%d], day[%d], uSecond[%d]",
                    SystemTime.uYear,SystemTime.uMonth, SystemTime.uMinute, SystemTime.uHour, SystemTime.uDayOfWeek,
                    SystemTime.uDay, SystemTime.uSecond);
                SL_Sleep(5000);

                SystemTime.uYear = 2013;
                SystemTime.uMonth = 10;
                SystemTime.uDayOfWeek = 6;
                SystemTime.uDay = 12;
                SystemTime.uHour = 16;
                SystemTime.uMinute = 54;
                SystemTime.uSecond = 50;
                SystemTime.uMilliseconds = 0;
                
                ret = SL_SetLocalTime(&SystemTime);
                SL_ApiPrint("SLAPP: ret[%d]", ret);
                
                SL_Sleep(5000);
                //SL_AppStartAgps();
                SL_GetLocalTime(&SystemTime1);
                SL_ApiPrint("SLAPP: year[%d], mon[%d], min[%d], hour[%d], dweek[%d], day[%d], uSecond[%d]",
                    SystemTime1.uYear,SystemTime1.uMonth, SystemTime1.uMinute, SystemTime1.uHour, SystemTime1.uDayOfWeek,
                    SystemTime1.uDay, SystemTime1.uSecond);
                SL_Sleep(5000);
                SL_AppStartAgps();
                stSltask.element[0] = SL_GetAppTaskHandle();
                SL_StartTimer(stSltask, APP_TIMER_CALL_RING, SL_TIMER_MODE_PERIODIC, SL_SecondToTicks(900));
#endif
            }
            break;
            case EVT_APP_GPRS_READY:
            {
                SL_AppStartTcpip();
            }
            break;
            case EVT_APP_TCP_CLOSE:
            {
                SL_ApiPrint("SLAPP: resv EVT_APP_TCP_CLOSE socket[%d]", ev.nParam1);
                SL_TcpipSocketClose(ev.nParam1);
            }
            break;            
            case SL_EV_UART_RECEIVE_DATA_IND:
            {   
                pUartData = (PSL_UART_DATA)ev.nParam1;
                ulUartId = ev.nParam2;

                //gucTaskId = SL_TakeMutex(gucMutex);
                //SL_Memset(gucUartRecvBuff, 0, sizeof(gucUartRecvBuff));
                if(gucUartRecvBuffCurPosi + pUartData->ulDataLen > sizeof(gucUartRecvBuff))
                {
                    SL_Memset(gucUartRecvBuff, 0, sizeof(gucUartRecvBuff));
                    gucUartRecvBuffCurPosi = 0;
                }
                
                SL_Memcpy(gucUartRecvBuff + gucUartRecvBuffCurPosi, pUartData->aucDataBuf, pUartData->ulDataLen);
                SL_ApiPrint("SLAPP: uart[%d] read uart lenth[%d]", ulUartId, pUartData->ulDataLen);
                gucUartRecvBuffCurPosi += pUartData->ulDataLen;
                
                //SL_GiveMutex(gucMutex, gucTaskId);
            }
            break;
            case SL_EV_TIMER:
            {
                if(ev.nParam1 == APP_TIMER_CALL_RING)
                {
                    SL_AppAnswer();
                }
                
                else if(ev.nParam1 == APP_TIMER_CALL_HANGUP)
                {
                    SL_AppSmsTest();
                    SL_Print("SLAPP: test1");
                }
                else if(ev.nParam1 == APP_TIMER_TCPIP_RESEND)
                {
                    SL_AppTcpipRsnd(0);
                }
#ifdef TEST_GPS               
                else if(ev.nParam1 == GPS_TIME_ID)
                {
                    SL_ApiPrint("SLAPP: GPS timer timeout!!");
                    SL_GpsGet(2,Sl_gps_result);
                }
#endif                
            }
            break;
            case SL_EV_UART1_WKUP_IRQ_IND:
            {
                SL_Print("****** SL_EV_UART1_WKUP_IRQ_IND ******\n");
                SL_LpwrEnterWakeup();
                SL_UartOpen(SL_UART_1);
            }                
            break;
            
            default:
            break;
        }
    }
}

