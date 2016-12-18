#ifndef _SL_APP_MQTTCLIENT_H_
#define _SL_APP_MQTTCLIENT_H_

void MQTTConnect();

void MQTTDisconnect();

void MQTTPublish(const char *topic, const U8 *payload);

void MQTTSubscribe(const char *topic);

void MQTTUnsubscribe(const char *topic);

void MQTTSetAlias(const char *alias);

#endif
