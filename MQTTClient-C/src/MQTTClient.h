/*******************************************************************************
 * Copyright (c) 2014, 2015 IBM Corp.
 *
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * and Eclipse Distribution License v1.0 which accompany this distribution.
 *
 * The Eclipse Public License is available at
 *    http://www.eclipse.org/legal/epl-v10.html
 * and the Eclipse Distribution License is available at
 *   http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * Contributors:
 *    Allan Stockdill-Mander/Ian Craggs - initial API and implementation and/or initial documentation
 *    Ian Craggs - documentation and platform specific header
 *******************************************************************************/

#if !defined(__MQTT_CLIENT_C_)
#define __MQTT_CLIENT_C_

#if defined(__cplusplus)
 extern "C" {
#endif

#if defined(WIN32_DLL) || defined(WIN64_DLL)
  #define DLLImport __declspec(dllimport)
  #define DLLExport __declspec(dllexport)
#elif defined(LINUX_SO)
  #define DLLImport extern
  #define DLLExport  __attribute__ ((visibility ("default")))
#else
  #define DLLImport
  #define DLLExport
#endif

#include "MQTTPacket.h"
#include "stdio.h"
#include "cJSON.h"

#if defined(MQTTCLIENT_PLATFORM_HEADER)
/* The following sequence of macros converts the MQTTCLIENT_PLATFORM_HEADER value
 * into a string constant suitable for use with include.
 */
#define xstr(s) str(s)
#define str(s) #s
#include xstr(MQTTCLIENT_PLATFORM_HEADER)
#endif

#define MAX_PACKET_ID 65535 /* according to the MQTT specification - do not change! */

#if !defined(MAX_MESSAGE_HANDLERS)
#define MAX_MESSAGE_HANDLERS 5 /* redefinable - how many subscriptions do you want? */
#endif

enum QoS { QOS0, QOS1, QOS2 };

/* all failure return codes must be negative */
enum returnCode { BUFFER_OVERFLOW = -2, FAILURE = -1, SUCCESS = 0 };

/* The Platform specific header must define the Network and Timer structures and functions
 * which operate on them.
 *
typedef struct Network
{
	int (*mqttread)(Network*, unsigned char* read_buffer, int, int);
	int (*mqttwrite)(Network*, unsigned char* send_buffer, int, int);
} Network;*/

/* The Timer structure must be defined in the platform specific header,
 * and have the following functions to operate on it.  */
extern void TimerInit(Timer*);
extern char TimerIsExpired(Timer*);
extern void TimerCountdownMS(Timer*, unsigned int);
extern void TimerCountdown(Timer*, unsigned int);
extern int TimerLeftMS(Timer*);

typedef struct MQTTMessage
{
    enum QoS qos;
    unsigned char retained;
    unsigned char dup;
    uint64_t id;
    void *payload;
    size_t payloadlen;
} MQTTMessage;

typedef struct MessageData
{
    MQTTMessage* message;
    MQTTString* topicName;
} MessageData;

typedef struct {
        /* in MQTT v3.1,If the Client ID contains more than 23 characters, the server responds to
         * the CONNECT message with a CONNACK return code 2: Identifier Rejected.
         * */
        char client_id[200];
        /* in MQTT v3.1, it is recommended that passwords are kept to 12 characters or fewer, but
         * it is not required. */
        char username[200];
        /*in MQTT v3.1, It is recommended that passwords are kept to 12 characters or fewer, but
         * it is not required. */
        char password[200];
        /* user define it, and change size of device id. */
        char device_id[200];
} REG_info;


typedef void (*messageHandler)(MessageData*);
typedef void (*extendedmessageHandler)(EXTED_CMD cmd, int status, int ret_string_len, char *ret_string);

typedef struct MQTTClient
{
    uint64_t next_packetid;
    unsigned int command_timeout_ms;
    size_t buf_size,
      readbuf_size;
    unsigned char *buf,
      *readbuf;
    unsigned int keepAliveInterval;
    char ping_outstanding;
    int isconnected;

    struct MessageHandlers
    {
        const char* topicFilter;
        void (*fp) (MessageData*);
    } messageHandlers[MAX_MESSAGE_HANDLERS];      /* Message handlers are indexed by subscription topic */
    
    struct ExtMessageHandlers
    {
    	EXTED_CMD cmd;
        void (*cb) (EXTED_CMD cmd, int status, int ret_string_len, char *ret_string);
    } extmessageHandlers[MAX_MESSAGE_HANDLERS];      /* Message handlers are indexed by subscription topic */

    void (*defaultMessageHandler) (MessageData*);

    Network* ipstack;
    Timer ping_timer;
#if defined(MQTT_TASK)
	Mutex mutex;
	Thread thread;
#endif 
} MQTTClient;

#define DefaultClient {0, 0, 0, 0, NULL, NULL, 0, 0, 0}


/**
 * Create an MQTT client object
 * @param client
 * @param network
 * @param command_timeout_ms
 * @param
 */
DLLExport void MQTTClientInit(MQTTClient* client, Network* network, unsigned int command_timeout_ms,
		unsigned char* sendbuf, size_t sendbuf_size, unsigned char* readbuf, size_t readbuf_size);

/** MQTT Connect - send an MQTT connect packet down the network and wait for a Connack
 *  The nework object must be connected to the network endpoint before calling this
 *  @param options - connect options
 *  @return success code
 */
DLLExport int MQTTConnect(MQTTClient* client, MQTTPacket_connectData* options);

/** MQTT Publish - send an MQTT publish packet and wait for all acks to complete for all QoSs
 *  @param client - the client object to use
 *  @param topic - the topic to publish to
 *  @param message - the message to send
 *  @return success code
 */
DLLExport int MQTTPublish(MQTTClient* client, const char*, MQTTMessage*);

/** MQTT Subscribe - send an MQTT subscribe packet and wait for suback before returning.
 *  @param client - the client object to use
 *  @param topicFilter - the topic filter to subscribe to
 *  @param message - the message to send
 *  @return success code
 */
DLLExport int MQTTSubscribe(MQTTClient* client, const char* topicFilter, enum QoS, messageHandler);

/** MQTT Subscribe - send an MQTT unsubscribe packet and wait for unsuback before returning.
 *  @param client - the client object to use
 *  @param topicFilter - the topic filter to unsubscribe from
 *  @return success code
 */
DLLExport int MQTTUnsubscribe(MQTTClient* client, const char* topicFilter);

/** MQTT Disconnect - send an MQTT disconnect packet and close the connection
 *  @param client - the client object to use
 *  @return success code
 */
DLLExport int MQTTDisconnect(MQTTClient* client);

/** MQTT Yield - MQTT background
 *  @param client - the client object to use
 *  @param time - the time, in milliseconds, to yield for 
 *  @return success code
 */
DLLExport int MQTTYield(MQTTClient* client, int time);


DLLExport int MQTTClient_get_host_v2(char *appkey, char* url);
DLLExport int MQTTClient_get_host(char *appkey, char* url);
DLLExport int MQTTClient_setup_with_appkey(char* appkey, REG_info *info);
DLLExport int MQTTClient_setup_with_appkey_v2(char* appkey, REG_info *info);
DLLExport int MQTTClient_setup_with_appkey_and_deviceid(char* appkey, char *deviceid, REG_info *info);
DLLExport int MQTTClient_setup_with_appkey_and_deviceid_v2(char* appkey, char *deviceid, REG_info *info);

DLLExport int MQTTSetAlias(MQTTClient*, const char*);
DLLExport int MQTTPublishToAlias(MQTTClient* c, const char* alias, void *payload, int payloadlen);
DLLExport int MQTTReport(MQTTClient* c, const char* action, const char *data);
DLLExport int MQTTGetAlias(MQTTClient* c, const char *param);
DLLExport int MQTTGetTopic(MQTTClient* c, const char *parameter);
DLLExport int MQTTGetStatus(MQTTClient* c, const char *parameter);
DLLExport int MQTTGetAliasList(MQTTClient* c, const char *parameter);
DLLExport int MQTTSetCallBack(MQTTClient *c, messageHandler, extendedmessageHandler);

DLLExport int MQTTPublish2(MQTTClient* c,
		const char* topicName, void* payload, int payloadlen, cJSON *opt);
DLLExport int MQTTPublish2ToAlias(MQTTClient* c,
				const char* alias, void* payload, int payloadlen, cJSON *opt);

DLLExport int MQTTGetTopic2(MQTTClient* c, const char *alias);
DLLExport int MQTTGetStatus2(MQTTClient* c, const char *alias);
DLLExport int MQTTGetAliasList2(MQTTClient* c, const char *topic);

DLLExport void setDefaultMessageHandler(MQTTClient*, messageHandler);



#if defined(MQTT_TASK)
/** MQTT start background thread for a client.  After this, MQTTYield should not be called.
*  @param client - the client object to use
*  @return success code
*/
DLLExport int MQTTStartTask(MQTTClient* client);
#endif

#if defined(__cplusplus)
     }
#endif

#endif
