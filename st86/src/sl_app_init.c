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

#define BUZZER_TIME_ID 200
#define APP_TASK1_STACK_SIZE    (2 * 2048)
#define APP_TASK1_PRIORITY      (SL_APP_TASK_PRIORITY_BEGIN + 1)

/* variable */ 
HANDLE g_SLApp1 = HNULL;

static U8 gucUartRecvBuff[100] = {0};
static U8 gucUartRecvBuffCurPosi = 0;

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
                // if(ev.nParam1 == APP_TIMER_AUDIO_REC)
                // {
                //     SL_AppAudioRecStop();
                // }
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
}

void APP_ENTRY_START SL_Entry(void)
{
    SL_EVENT ev = {0};
    SL_TASK stSltask;
    PSL_UART_DATA pUartData;
    U32 ulUartId;

    SL_Memset(&ev, 0, sizeof(SL_EVENT));
    SL_AppCreateTask();

    stSltask.element[0] = SL_GetAppTaskHandle();
    SL_AppSendMsg(stSltask.element[0], EVT_APP_READY, 0);


    SL_GpioSetDir(SL_GPIO_5, SL_GPIO_OUT);
    SL_StartTimer(stSltask, BUZZER_TIME_ID, SL_TIMER_MODE_PERIODIC, SL_SecondToTicks(10));

    while(1)
    {
        SL_FreeMemory((VOID*)ev.nParam1);
        SL_GetEvent(stSltask, &ev);
        SL_ApiPrint("SLAPP: SL_Entry get event[%d]\n", ev.nEventId);
        switch (ev.nEventId)
        {
            case EVT_APP_READY:
            {
                // SL_AppInitTcpip();

                while(SL_GetNwStatus() != SL_RET_OK)
                {
                    SL_ApiPrint("SLAPP: net register");
                    SL_Sleep(1000);
                }
                //#endif
                
                SL_ApiPrint("SLAPP: net register ok");
                SL_Sleep(3000);
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
                if(ev.nParam1 == BUZZER_TIME_ID)
                {
                    SL_GpioWrite(SL_GPIO_5, SL_PIN_LOW);
                    SL_Sleep(1000);
                    SL_GpioWrite(SL_GPIO_5, SL_PIN_HIGH);
                }     
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
