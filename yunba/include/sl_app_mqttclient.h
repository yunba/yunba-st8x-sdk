#ifndef _SL_APP_MQTTCLIENT_H_
#define _SL_APP_MQTTCLIENT_H_

void MQTTInit(HANDLE stTask);

void MQTTUnInit();

void MQTTConnect();

void MQTTDisconnect();

void MQTTPublish(const char *topic, const U8 *payload);

void MQTTSubscribe(const char *topic);

void MQTTUnSubscribe(const char *topic);

void MQTTSetAlias(const char *alias);

#endif
