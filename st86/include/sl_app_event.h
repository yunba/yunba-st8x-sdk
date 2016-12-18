#ifndef _SL_APP_EVENT_H_
#define _SL_APP_EVENT_H_

#include "sl_event.h"
/* events */ 
#define EVT_APP_READY                   (SL_EV_MMI_EV_BASE + 1)
#define EVT_APP_GPRS_READY              (EVT_APP_READY + 1)
#define EVT_APP_GPRS_STARTRECV          (EVT_APP_GPRS_READY + 1)
#define EVT_APP_SOCKET_SETUP            (EVT_APP_GPRS_STARTRECV + 1)
#define EVT_APP_GPRS_STARTSEND          (EVT_APP_SOCKET_SETUP + 1)
#define EVT_APP_GPRS_CONNECT            (EVT_APP_GPRS_STARTSEND + 1)
#define EVT_APP_TCP_CLOSE               (EVT_APP_GPRS_CONNECT + 1)
#define EVT_APP_CREATE_FILE             (EVT_APP_TCP_CLOSE + 1)
#define EVT_APP_READ_FILE               (EVT_APP_CREATE_FILE + 1)
#define EVT_APP_PLAY_FILE               (EVT_APP_READ_FILE + 1)
#define EVT_APP_AGPS                    (EVT_APP_PLAY_FILE + 1)
#define EVT_APP_DTMF                    (EVT_APP_AGPS + 1)
#define EVT_APP_PLAY_TTS                (EVT_APP_DTMF + 1)
#define SL_APP_FTPOP_REQ                (EVT_APP_PLAY_TTS + 1)
#define SL_APP_UPDATE_REQ               (SL_APP_FTPOP_REQ + 1)

#define EVT_APP_UNLOCK					(SL_APP_UPDATE_REQ + 1)
#define EVT_APP_LOCK					(EVT_APP_UNLOCK + 1)
#define EVT_APP_REPORT_STATUS           (EVT_APP_LOCK + 1)
#define EVT_APP_MQTT_ERROR              (EVT_APP_REPORT_STATUS + 1)
#define EVT_APP_MQTT_INIT_OK            (EVT_APP_MQTT_ERROR + 1)
#define EVT_APP_MQTT_CONNACK            (EVT_APP_MQTT_INIT_OK + 1)
#define EVT_APP_MQTT_PUBLISH            (EVT_APP_MQTT_CONNACK + 1)
#define EVT_APP_MQTT_EXTCMD             (EVT_APP_MQTT_PUBLISH + 1)

#endif
