#ifndef _SL_APP_MQTTCLIENT_H_
#define _SL_APP_MQTTCLIENT_H_

void MQTTInit(HANDLE stTask);

void MQTTUnInit();

void MQTTConnect();

void MQTTDisconnect();

void MQTTPublish(char *topic, U8 *payload);

void MQTTSubscribe(char *topic);

void MQTTUnSubscribe(char *topic);

void MQTTSetAlias(char *alias);

#endif
