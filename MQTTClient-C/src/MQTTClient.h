/*******************************************************************************
 * Copyright (c) 2014 IBM Corp.
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
 *******************************************************************************/

#ifndef __MQTT_CLIENT_C_
#define __MQTT_CLIENT_C_

#include "MQTTPacket.h"
#include "stdio.h"
#include "cJSON.h"
#include "" //Platform specific implementation header file

#define MAX_PACKET_ID 65535
#define MAX_MESSAGE_HANDLERS 5

enum QoS { QOS0, QOS1, QOS2 };


// all failure return codes must be negative
enum returnCode { BUFFER_OVERFLOW = -2, FAILURE = -1, SUCCESS = 0 };

void NewTimer(Timer*);

typedef struct MQTTMessage MQTTMessage;

typedef struct MessageData MessageData;

struct MQTTMessage
{
    enum QoS qos;
    char retained;
    char dup;
    uint64_t id;
    void *payload;
    size_t payloadlen;
};

struct MessageData
{
    MQTTMessage* message;
    MQTTString* topicName;
};

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

typedef struct Client Client;

int MQTTConnect (Client*, MQTTPacket_connectData*);
int MQTTPublish (Client*, const char*, MQTTMessage*);
int MQTTSubscribe (Client*, const char*, enum QoS);
int MQTTUnsubscribe (Client*, const char*);
int MQTTDisconnect (Client*);
int MQTTYield (Client*, int);

int MQTTClient_get_host_v2(char *appkey, char* url);
int MQTTClient_get_host(char *appkey, char* url);
int MQTTClient_setup_with_appkey(char* appkey, REG_info *info);
int MQTTClient_setup_with_appkey_v2(char* appkey, REG_info *info);
int MQTTClient_setup_with_appkey_and_deviceid(char* appkey, char *deviceid, REG_info *info);
int MQTTClient_setup_with_appkey_and_deviceid_v2(char* appkey, char *deviceid, REG_info *info);

int MQTTSetAlias(Client*, const char*);
int MQTTPublishToAlias(Client* c, const char* alias, void *payload, int payloadlen);
int MQTTReport(Client* c, const char* action, const char *data);
int MQTTGetAlias(Client* c, const char *param);
int MQTTGetTopic(Client* c, const char *parameter);
int MQTTGetStatus(Client* c, const char *parameter);
int MQTTGetAliasList(Client* c, const char *parameter);
int MQTTSetCallBack(Client *c, messageHandler, extendedmessageHandler);

int MQTTPublish2(Client* c,
		const char* topicName, void* payload, int payloadlen, cJSON *opt);
int MQTTPublish2ToAlias(Client* c,
				const char* alias, void* payload, int payloadlen, cJSON *opt);

int MQTTGetTopic2(Client* c, const char *alias);
int MQTTGetStatus2(Client* c, const char *alias);
int MQTTGetAliasList2(Client* c, const char *topic);

void setDefaultMessageHandler(Client*, messageHandler);

void MQTTClient(Client*, Network*, unsigned int, unsigned char*, size_t, unsigned char*, size_t);

struct Client {
    uint64_t next_packetid;
    unsigned int command_timeout_ms;
    size_t buf_size, readbuf_size;
    unsigned char *buf;  
    unsigned char *readbuf; 
    unsigned int keepAliveInterval;
    char ping_outstanding;
    int isconnected;

    struct MessageHandlers
    {
        const char* topicFilter;
        void (*fp) (MessageData*);
    } messageHandlers[MAX_MESSAGE_HANDLERS];      // Message handlers are indexed by subscription topic
    
    struct ExtMessageHandlers
    {
    	EXTED_CMD cmd;
        void (*cb) (EXTED_CMD cmd, int status, int ret_string_len, char *ret_string);
    } extmessageHandlers[MAX_MESSAGE_HANDLERS];      // Message handlers are indexed by subscription topic

    void (*defaultMessageHandler) (MessageData*);
    
    Network* ipstack;
    Timer ping_timer;
};

#define DefaultClient {0, 0, 0, 0, NULL, NULL, 0, 0, 0}

#endif
