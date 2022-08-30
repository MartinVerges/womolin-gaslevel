/**
 * @file MQTTclient.cpp
 * @author Martin Verges <martin@verges.cc>
 * @brief MQTT client library
 * @version 0.1
 * @date 2022-05-29
**/

#include "log.h"

#include "MQTTclient.h"

bool enableMqtt = false;                    // Enable Mqtt, disable to reduce power consumtion, stored in NVS

MQTTclient::MQTTclient() {
  client.setClient(ethClient);
}
MQTTclient::~MQTTclient() {}

bool MQTTclient::isConnected() {
  return client.connected();
}

bool MQTTclient::isReady() {
  if (mqttTopic.length() > 0 && isConnected()) return true;
  else return false;
}

void MQTTclient::prepare(String host, uint16_t port, String topic, String user, String pass) {
  mqttHost = host;
  mqttPort = port;
  mqttTopic = topic;
  mqttUser = user;
  mqttPass = pass;

  // username+password will be used on connect()
  if (mqttUser.length() > 0 && mqttPass.length() > 0) {
      LOG_INFO(F("[MQTT] Configured broker user: "));
      LOG_INFO_LN(mqttUser);
      LOG_INFO_LN(F("[MQTT] Configured broker pass: **hidden**"));
  } else LOG_INFO_LN(F("[MQTT] Configured broker without user and password!"));

  if (mqttPort == 0 || mqttPort < 0 || mqttPort > 65535) mqttPort = 1883;
  LOG_INFO(F("[MQTT] Configured broker port: "));
  LOG_INFO_LN(mqttPort);

  IPAddress ip;
  if (ip.fromString(mqttHost)) { // this is a valid IP
    LOG_INFO(F("[MQTT] Configured broker IP: "));
    LOG_INFO_LN(ip);
    client.setServer(ip, mqttPort);
  } else {
    LOG_INFO(F("[MQTT] Configured broker host: "));
    LOG_INFO_LN(mqttHost);
    client.setServer(mqttHost.c_str(), mqttPort);
  }
}

void MQTTclient::connect() {
  if (!enableMqtt) {
    LOG_INFO_LN(F("[MQTT] disabled!"));
  } else {
    LOG_INFO_LN(F("[MQTT] Connecting to MQTT..."));
    client.connect(
      mqttClientId.c_str(),
      mqttUser.length() > 0 ? mqttUser.c_str() : NULL,
      mqttPass.length() > 0 ? mqttPass.c_str() : NULL,
      0,
      0,
      1,
      0,
      1
    );
    sleep(0.5);

    switch (client.state()) {
    case MQTT_CONNECTION_TIMEOUT:
      LOG_INFO_LN(F("[MQTT] ... connection time out"));
      break;
    case MQTT_CONNECTION_LOST:
      LOG_INFO_LN(F("[MQTT] ... connection lost"));
      break;
    case MQTT_CONNECT_FAILED:
      LOG_INFO_LN(F("[MQTT] ... connection failed"));
      break;
    case MQTT_DISCONNECTED:
      LOG_INFO_LN(F("[MQTT] ... disconnected"));
      break;
    case MQTT_CONNECTED:
      LOG_INFO_LN(F("[MQTT] ... connected"));
      break;
    case MQTT_CONNECT_BAD_PROTOCOL:
      LOG_INFO_LN(F("[MQTT] ... connection error: bad protocol"));
      break;
    case MQTT_CONNECT_BAD_CLIENT_ID:
      LOG_INFO_LN(F("[MQTT] ... connection error: bad client ID"));
      break;
    case MQTT_CONNECT_UNAVAILABLE:
      LOG_INFO_LN(F("[MQTT] ... connection error: unavailable"));
      break;
    case MQTT_CONNECT_BAD_CREDENTIALS:
      LOG_INFO_LN(F("[MQTT] ... connection error: bad credentials"));
      break;
    case MQTT_CONNECT_UNAUTHORIZED:
      LOG_INFO_LN(F("[MQTT] ... connection error: unauthorized"));
      break;
    default:
      LOG_INFO(F("[MQTT] ... connection error: unknown code "));
      LOG_INFO_LN(client.state());
      break;
    }
  }
}
void MQTTclient::disconnect() {
  client.disconnect();
}
