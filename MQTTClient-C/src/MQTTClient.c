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
 *******************************************************************************/
#include "cJSON.h"
#include "MQTTClient.h"

static void NewMessageData(MessageData* md, MQTTString* aTopicName, MQTTMessage* aMessage) {
    md->topicName = aTopicName;
    md->message = aMessage;
}


static uint64_t getNextPacketId(MQTTClient *c) {
    c->next_packetid = generate_uuid();
    return c->next_packetid;
}


static int sendPacket(MQTTClient* c, int length, Timer* timer)
{
    int rc = FAILURE, 
        sent = 0;
    
    while (sent < length && !TimerIsExpired(timer))
    {
        rc = c->ipstack->mqttwrite(c->ipstack, &c->buf[sent], length, TimerLeftMS(timer));
        if (rc < 0)  // there was an error writing the data
            break;
        sent += rc;
    }
    if (sent == length)
    {
        TimerCountdown(&c->ping_timer, c->keepAliveInterval); // record the fact that we have successfully sent the packet
        rc = SUCCESS;
    }
    else
        rc = FAILURE;
    return rc;
}


void MQTTClientInit(MQTTClient* c, Network* network, unsigned int command_timeout_ms,
		unsigned char* sendbuf, size_t sendbuf_size, unsigned char* readbuf, size_t readbuf_size)
{
    int i;
    c->ipstack = network;
    
    for (i = 0; i < MAX_MESSAGE_HANDLERS; ++i)
        c->messageHandlers[i].topicFilter = 0;
    c->command_timeout_ms = command_timeout_ms;
    c->buf = sendbuf;
    c->buf_size = sendbuf_size;
    c->readbuf = readbuf;
    c->readbuf_size = readbuf_size;
    c->isconnected = 0;
    c->ping_outstanding = 0;
    c->defaultMessageHandler = NULL;
	c->next_packetid = 1;
    TimerInit(&c->ping_timer);
#if defined(MQTT_TASK)
	MutexInit(&c->mutex);
#endif
}


static int decodePacket(MQTTClient* c, int* value, int timeout)
{
    unsigned char i;
    int multiplier = 1;
    int len = 0;
    const int MAX_NO_OF_REMAINING_LENGTH_BYTES = 4;

    *value = 0;
    do
    {
        int rc = MQTTPACKET_READ_ERROR;

        if (++len > MAX_NO_OF_REMAINING_LENGTH_BYTES)
        {
            rc = MQTTPACKET_READ_ERROR; /* bad data */
            goto exit;
        }
        rc = c->ipstack->mqttread(c->ipstack, &i, 1, timeout);
        if (rc != 1)
            goto exit;
        *value += (i & 127) * multiplier;
        multiplier *= 128;
    } while ((i & 128) != 0);
exit:
    return len;
}


static int readPacket(MQTTClient* c, Timer* timer)
{
    int rc = FAILURE;
    MQTTHeader header = {0};
    int len = 0;
    int rem_len = 0;

    /* 1. read the header byte.  This has the packet type in it */
    if (c->ipstack->mqttread(c->ipstack, c->readbuf, 1, TimerLeftMS(timer)) != 1)
        goto exit;

    len = 1;
    /* 2. read the remaining length.  This is variable in itself */
    decodePacket(c, &rem_len, TimerLeftMS(timer));
    len += MQTTPacket_encode(c->readbuf + 1, rem_len); /* put the original remaining length back into the buffer */

    /* 3. read the rest of the buffer using a callback to supply the rest of the data */
    if (rem_len > 0 && (c->ipstack->mqttread(c->ipstack, c->readbuf + len, rem_len, TimerLeftMS(timer)) != rem_len))
        goto exit;

    header.byte = c->readbuf[0];
    rc = header.bits.type;
exit:
    return rc;
}


// assume topic filter and name is in correct format
// # can only be at end
// + and # can only be next to separator
static char isTopicMatched(char* topicFilter, MQTTString* topicName)
{
    char* curf = topicFilter;
    char* curn = topicName->lenstring.data;
    char* curn_end = curn + topicName->lenstring.len;
    
    while (*curf && curn < curn_end)
    {
        if (*curn == '/' && *curf != '/')
            break;
        if (*curf != '+' && *curf != '#' && *curf != *curn)
            break;
        if (*curf == '+')
        {   // skip until we meet the next separator, or end of string
            char* nextpos = curn + 1;
            while (nextpos < curn_end && *nextpos != '/')
                nextpos = ++curn + 1;
        }
        else if (*curf == '#')
            curn = curn_end - 1;    // skip until end of string
        curf++;
        curn++;
    };
    
    return (curn == curn_end) && (*curf == '\0');
}


int deliverMessage(MQTTClient* c, MQTTString* topicName, MQTTMessage* message)
{
    int i;
    int rc = FAILURE;

    // we have to find the right message handler - indexed by topic
//    for (i = 0; i < MAX_MESSAGE_HANDLERS; ++i)
//    {
 //   	printf("%s, %s, %s\n", __func__, topicName->cstring, c->messageHandlers[i].topicFilter);
  //      if (c->messageHandlers[i].topicFilter != 0 && (MQTTPacket_equals(topicName, (char*)c->messageHandlers[i].topicFilter) ||
  //              isTopicMatched((char*)c->messageHandlers[i].topicFilter, topicName)))
        {
    //        if (c->messageHandlers[i].fp != NULL)
            {

                MessageData md;
                NewMessageData(&md, topicName, message);
                c->messageHandlers[0].fp(&md);
                rc = SUCCESS;
            }
        }
  //  }
    
//    if (rc == FAILURE && c->defaultMessageHandler != NULL)
//    {
//        MessageData md;
//        NewMessageData(&md, topicName, message);
//        c->defaultMessageHandler(&md);
//        rc = SUCCESS;
//    }
    
    return rc;
}

int deliverextMessage(MQTTClient* c, EXTED_CMD cmd, int status, int ret_string_len, char *ret_string)
{
		int i;
		int rc = SUCCESS;

	    // we have to find the right message handler - indexed by topic
//	    for (i = 0; i < MAX_MESSAGE_HANDLERS; ++i)
//	    {
//		if (c->extmessageHandlers[i].cb != NULL && c->extmessageHandlers[i].cmd > 0)
//	    	{
//			c->extmessageHandlers[i].cb(cmd, status, ret_string_len, ret_string);
//			rc = SUCCESS;
//			break;
//	    	}
//	    }
		c->extmessageHandlers[0].cb(cmd, status, ret_string_len, ret_string);

	    return rc;
}


int keepalive(MQTTClient* c)
{
    int rc = FAILURE;

    if (c->keepAliveInterval == 0)
    {
        rc = SUCCESS;
        goto exit;
    }

    if (TimerIsExpired(&c->ping_timer))
    {
	//FIXME: If no ping response, should disconnect and try re-connecting...
//        if (!c->ping_outstanding)
        {
	    int len;
            Timer timer;
            TimerInit(&timer);
            TimerCountdownMS(&timer, 1000);
	    len = MQTTSerialize_pingreq(c->buf, c->buf_size);
            if (len > 0 && (rc = sendPacket(c, len, &timer)) == SUCCESS) // send the ping packet
                c->ping_outstanding = 1;
        }
    }

exit:
    return rc;
}


int cycle(MQTTClient* c, Timer* timer)
{
    // read the socket, see what work is due
    unsigned short packet_type = readPacket(c, timer);

//    if (packet_type != 65535)
//    	printf("%s, %i\n", __func__, packet_type);
    
    int len = 0,
        rc = SUCCESS;

    switch (packet_type)
    {
        case CONNACK:
        case PUBACK:
        case SUBACK:
            break;
        case EXTCMD:
        {
            MQTTString topicName;
            MQTTMessage msg;
            EXTED_CMD cmd;
            int status;
            if (MQTTDeserialize_extendedcmd((unsigned char*)&msg.dup, (int*)&msg.qos, (unsigned char*)&msg.retained, (uint64_t*)&msg.id, &cmd,
               &status, (unsigned char**)&msg.payload, (int*)&msg.payloadlen, c->readbuf, c->readbuf_size) != 1)
                goto exit;
            deliverextMessage(c, cmd, status, msg.payloadlen, msg.payload);
        	break;
        }

        case PUBLISH:
        {
            MQTTString topicName;
            MQTTMessage msg;
            int intQoS;
            if (MQTTDeserialize_publish(&msg.dup, &intQoS, &msg.retained, (uint64_t*)&msg.id, &topicName,
               (unsigned char**)&msg.payload, (int*)&msg.payloadlen, c->readbuf, c->readbuf_size) != 1)
                goto exit;
            msg.qos = (enum QoS)intQoS;
            deliverMessage(c, &topicName, &msg);
            if (msg.qos != QOS0)
            {
                if (msg.qos == QOS1)
                    len = MQTTSerialize_ack(c->buf, c->buf_size, PUBACK, 0, msg.id);
                else if (msg.qos == QOS2)
                    len = MQTTSerialize_ack(c->buf, c->buf_size, PUBREC, 0, msg.id);
                if (len <= 0)
                    rc = FAILURE;
                else
                    rc = sendPacket(c, len, timer);
                if (rc == FAILURE)
                    goto exit; // there was a problem
            }
            break;
        }
        case PUBREC:
        {
            uint64_t mypacketid;
            unsigned char dup, type;
            if (MQTTDeserialize_ack(&type, &dup, &mypacketid, c->readbuf, c->readbuf_size) != 1)
                rc = FAILURE;
            else if ((len = MQTTSerialize_ack(c->buf, c->buf_size, PUBREL, 0, mypacketid)) <= 0)
                rc = FAILURE;
            else if ((rc = sendPacket(c, len, timer)) != SUCCESS) // send the PUBREL packet
                rc = FAILURE; // there was a problem
            if (rc == FAILURE)
                goto exit; // there was a problem
            break;
        }
        case PUBCOMP:

            break;

        case PUBREL:
        {
            uint64_t mypacketid;
            unsigned char dup, type;
             if (MQTTDeserialize_ack(&type, &dup, &mypacketid, c->readbuf, c->readbuf_size) != 1)
                 rc = FAILURE;
             else if ((len = MQTTSerialize_ack(c->buf, c->buf_size, PUBCOMP, 0, mypacketid)) <= 0)
                 rc = FAILURE;
             else if ((rc = sendPacket(c, len, timer)) != SUCCESS) // send the PUBREL packet
                 rc = FAILURE; // there was a problem
             if (rc == FAILURE)
                 goto exit; // there was a problem
         }
        break;

        case PINGRESP:
            c->ping_outstanding = 0;
            break;
    }
    keepalive(c);
exit:
    if (rc == SUCCESS)
        rc = packet_type;
    return rc;
}


int MQTTYield(MQTTClient* c, int timeout_ms)
{
    int rc = SUCCESS;
    Timer timer;

    TimerInit(&timer);
    TimerCountdownMS(&timer, timeout_ms);

	do
    {
        if (cycle(c, &timer) == FAILURE)
        {
            rc = FAILURE;
            break;
        }
	} while (!TimerIsExpired(&timer));
        
    return rc;
}


void MQTTRun(void* parm)
{
	Timer timer;
	MQTTClient* c = (MQTTClient*)parm;

	TimerInit(&timer);

	while (1)
	{
#if defined(MQTT_TASK)
		MutexLock(&c->mutex);
#endif
		TimerCountdownMS(&timer, 500); /* Don't wait too long if no traffic is incoming */
		cycle(c, &timer);
#if defined(MQTT_TASK)
		MutexUnlock(&c->mutex);
#endif
	} 
}


#if defined(MQTT_TASK)
int MQTTStartTask(MQTTClient* client)
{
	return ThreadStart(&client->thread, &MQTTRun, client);
}
#endif


int waitfor(MQTTClient* c, int packet_type, Timer* timer)
{
    int rc = FAILURE;
    
    do
    {
        if (TimerIsExpired(timer))
            break; // we timed out
    }
    while ((rc = cycle(c, timer)) != packet_type);  
    
    return rc;
}


int MQTTConnect(MQTTClient* c, MQTTPacket_connectData* options)
{
    Timer connect_timer;
    int rc = FAILURE;
    MQTTPacket_connectData default_options = MQTTPacket_connectData_initializer;
    int len = 0;

#if defined(MQTT_TASK)
	MutexLock(&c->mutex);
#endif
	if (c->isconnected) /* don't send connect packet again if we are already connected */
		goto exit;
    
    TimerInit(&connect_timer);
    TimerCountdownMS(&connect_timer, c->command_timeout_ms);

    if (options == 0)
        options = &default_options; /* set default options if none were supplied */
    
    c->keepAliveInterval = options->keepAliveInterval;
    TimerCountdown(&c->ping_timer, c->keepAliveInterval);
    if ((len = MQTTSerialize_connect(c->buf, c->buf_size, options)) <= 0)
        goto exit;
    if ((rc = sendPacket(c, len, &connect_timer)) != SUCCESS)  // send the connect packet
        goto exit; // there was a problem
    
    // this will be a blocking call, wait for the connack
    if (waitfor(c, CONNACK, &connect_timer) == CONNACK)
    {
        unsigned char connack_rc = 255;
        unsigned char sessionPresent = 0;
        if (MQTTDeserialize_connack(&sessionPresent, &connack_rc, c->readbuf, c->readbuf_size) == 1)
            rc = connack_rc;
        else
            rc = FAILURE;
    }
    else
        rc = FAILURE;
    
exit:
    if (rc == SUCCESS)
        c->isconnected = 1;

#if defined(MQTT_TASK)
	MutexUnlock(&c->mutex);
#endif

    return rc;
}


int MQTTSubscribe(MQTTClient* c, const char* topicFilter, enum QoS qos, messageHandler messageHandler)
{ 
    int rc = FAILURE;  
    Timer timer;
    int len = 0;
    MQTTString topic = MQTTString_initializer;
    topic.cstring = (char *)topicFilter;
    
#if defined(MQTT_TASK)
	MutexLock(&c->mutex);
#endif
	if (!c->isconnected)
		goto exit;

    TimerInit(&timer);
    TimerCountdownMS(&timer, c->command_timeout_ms);
    
    len = MQTTSerialize_subscribe(c->buf, c->buf_size, 0, getNextPacketId(c), 1, &topic, (int*)&qos);
    if (len <= 0)
        goto exit;
    if ((rc = sendPacket(c, len, &timer)) != SUCCESS) // send the subscribe packet
        goto exit;             // there was a problem
    
    if (waitfor(c, SUBACK, &timer) == SUBACK)      // wait for suback 
    {
        int count = 0, grantedQoS = -1;
        uint64_t mypacketid;
        if (MQTTDeserialize_suback(&mypacketid, 1, &count, &grantedQoS, c->readbuf, c->readbuf_size) == 1)
            rc = grantedQoS; // 0, 1, 2 or 0x80 
    }
    else 
        rc = FAILURE;
        
exit:
#if defined(MQTT_TASK)
	MutexUnlock(&c->mutex);
#endif
    return rc;
}


int MQTTUnsubscribe(MQTTClient* c, const char* topicFilter)
{   
    int rc = FAILURE;
	int len = 0;

    Timer timer;    
    MQTTString topic = MQTTString_initializer;
    topic.cstring = (char *)topicFilter;

#if defined(MQTT_TASK)
	MutexLock(&c->mutex);
#endif
	if (!c->isconnected)
		goto exit;

    TimerInit(&timer);
    TimerCountdownMS(&timer, c->command_timeout_ms);
    
    if ((len = MQTTSerialize_unsubscribe(c->buf, c->buf_size, 0, getNextPacketId(c), 1, &topic)) <= 0)
        goto exit;
    if ((rc = sendPacket(c, len, &timer)) != SUCCESS) // send the subscribe packet
        goto exit; // there was a problem
    
    if (waitfor(c, UNSUBACK, &timer) == UNSUBACK)
    {
        uint64_t mypacketid;  // should be the same as the packetid above
        if (MQTTDeserialize_unsuback(&mypacketid, c->readbuf, c->readbuf_size) == 1)
            rc = 0; 
    }
    else
        rc = FAILURE;
    
exit:
#if defined(MQTT_TASK)
	MutexUnlock(&c->mutex);
#endif
    return rc;
}


int MQTTPublish(MQTTClient* c, const char* topicName, MQTTMessage* message)
{
    int rc = FAILURE;
	int len = 0;
    Timer timer;   
    MQTTString topic = MQTTString_initializer;
    topic.cstring = (char *)topicName;

#if defined(MQTT_TASK)
	MutexLock(&c->mutex);
#endif
	if (!c->isconnected)
		goto exit;

    TimerInit(&timer);
    TimerCountdownMS(&timer, c->command_timeout_ms);

    if (message->qos == QOS1 || message->qos == QOS2)
        message->id = getNextPacketId(c);
    
    len = MQTTSerialize_publish(c->buf, c->buf_size, 0, message->qos, message->retained, message->id, 
              topic, (unsigned char*)message->payload, message->payloadlen);
    if (len <= 0)
        goto exit;
    if ((rc = sendPacket(c, len, &timer)) != SUCCESS) // send the subscribe packet
        goto exit; // there was a problem
    
    if (message->qos == QOS1)
    {
        if (waitfor(c, PUBACK, &timer) == PUBACK)
        {
            uint64_t mypacketid;
            unsigned char dup, type;
            if (MQTTDeserialize_ack(&type, &dup, &mypacketid, c->readbuf, c->readbuf_size) != 1)
                rc = FAILURE;
        }
        else
            rc = FAILURE;
    }
    else if (message->qos == QOS2)
    {
        if (waitfor(c, PUBCOMP, &timer) == PUBCOMP)
        {
        	uint64_t mypacketid;
            unsigned char dup, type;
            if (MQTTDeserialize_ack(&type, &dup, &mypacketid, c->readbuf, c->readbuf_size) != 1)
                rc = FAILURE;
        }
        else
            rc = FAILURE;
    }
    
exit:
#if defined(MQTT_TASK)
	MutexUnlock(&c->mutex);
#endif
    return rc;
}


int MQTTDisconnect(MQTTClient* c)
{  
    int rc = FAILURE;
    Timer timer;     // we might wait for incomplete incoming publishes to complete
    int len = 0;

#if defined(MQTT_TASK)
	MutexLock(&c->mutex);
#endif
    TimerInit(&timer);
    TimerCountdownMS(&timer, c->command_timeout_ms);

	len = MQTTSerialize_disconnect(c->buf, c->buf_size);
    if (len > 0)
        rc = sendPacket(c, len, &timer);            // send the disconnect packet
        
    c->isconnected = 0;

#if defined(MQTT_TASK)
	MutexUnlock(&c->mutex);
#endif
    return rc;
}

int MQTTSetAlias(MQTTClient* c, const char* alias)
{
	int rc = 0;
	/*TODO: buffer size ?? */
	char temp[100];
	MQTTMessage M;
	M.qos = 1;
	strcpy(temp, alias);
	M.payload = temp;
	M.id = getNextPacketId(c);
	M.payloadlen = strlen(temp);
	rc = MQTTPublish(c, ",yali", &M);
	return rc;
}

int MQTTPublishToAlias(MQTTClient* c, const char* alias, void *payload, int payloadlen)
{
	int rc = 0;
	/*TODO: buffer size ?? */
	char topic[100];
	MQTTMessage M;
	M.qos = 1;
	sprintf(topic, ",yta/%s", alias);
	M.payload = payload;
	M.id = getNextPacketId(c);
	M.payloadlen = payloadlen;
	rc = MQTTPublish(c, topic, &M);
	return rc;
}

int MQTTReport(MQTTClient* c, const char* action, const char *data)
{
	int rc = 0;
	/*TODO: buffer size ?? */
	char topic[100];
	MQTTMessage M;
	M.qos = 1;
	M.payload = (void *)data;
	M.id = getNextPacketId(c);
	M.payloadlen = strlen(data);
	sprintf(topic, "$$report/%s", action);
	rc = MQTTPublish(c, topic, &M);
	return rc;
}

#define DEFAULT_QOS 1
#define DEFAULT_RETAINED 0
int MQTTExtendedCmd(MQTTClient* c, EXTED_CMD cmd, void *payload, int payload_len, int qos, unsigned char retained)
{
    int rc = FAILURE;
    Timer timer;
    int len = 0;
    uint64_t id = 0;

    TimerInit(&timer);
    TimerCountdownMS(&timer, c->command_timeout_ms);

    if (!c->isconnected)
        goto exit;

    if (qos == QOS1 || qos == QOS2)
        id = getNextPacketId(c);

    len = MQTTSerialize_extendedcmd(c->buf, c->buf_size, 0, qos, retained, id,
    		cmd, payload, payload_len);

    if (len <= 0)
        goto exit;
    if ((rc = sendPacket(c, len, &timer)) != SUCCESS) // send the subscribe packet
        goto exit; // there was a problem

    if (waitfor(c, EXTCMD, &timer) == EXTCMD) {
    	rc = SUCCESS;
    }

exit:
    return rc;
}

int MQTTSetCallBack(MQTTClient *c, messageHandler cb, extendedmessageHandler ext_cb)
{
	int rc = SUCCESS;

	c->extmessageHandlers[0].cmd = 1;
	c->extmessageHandlers[0].cb = ext_cb;

	c->messageHandlers[0].topicFilter = 0;
	c->messageHandlers[0].fp = cb;

    return rc;
}

int MQTTGetAlias(MQTTClient* c, const char *param)
{
	int rc = MQTTExtendedCmd(c, GET_ALIAS, (void *)param, strlen(param), DEFAULT_QOS, DEFAULT_RETAINED);
	return rc;
}

int MQTTGetTopic(MQTTClient* c, const char *parameter)
{
	int rc = MQTTExtendedCmd(c, GET_TOPIC, (void *)parameter, strlen(parameter), DEFAULT_QOS, DEFAULT_RETAINED);
	return rc;
}

int MQTTGetStatus(MQTTClient* c, const char *parameter)
{
	int rc = MQTTExtendedCmd(c, GET_STATUS, (void *)parameter, strlen(parameter), DEFAULT_QOS, DEFAULT_RETAINED);
	return rc;
}

int MQTTGetAliasList(MQTTClient* c, const char *parameter)
{
	int rc = MQTTExtendedCmd(c, GET_ALIAS_LIST, (void *)parameter, strlen(parameter), DEFAULT_QOS, DEFAULT_RETAINED);
	return rc;
}

int MQTTGetTopic2(MQTTClient* c, const char *alias)
{
	int rc = MQTTExtendedCmd(c, GET_TOPIC_LIST2, (void *)alias, strlen(alias), DEFAULT_QOS, DEFAULT_RETAINED);
	return rc;
}

int MQTTGetStatus2(MQTTClient* c, const char *alias)
{
	int rc = MQTTExtendedCmd(c, GET_STATUS2, (void *)alias, strlen(alias), DEFAULT_QOS, DEFAULT_RETAINED);
	return rc;
}

int MQTTGetAliasList2(MQTTClient* c, const char *topic)
{
	int rc = MQTTExtendedCmd(c, GET_ALIASLIST2, (void *)topic, strlen(topic), DEFAULT_QOS, DEFAULT_RETAINED);
	return rc;
}

int MQTTPublish2(MQTTClient* c,
		const char* topicName, void* payload, int payloadlen, cJSON *opt)
{
	const char *key[PUBLISH2_TLV_MAX_NUM] =
	{"topic", "payload", "platform", "time_to_live", "time_delay", "location", "qos", "apn_json"};
	uint8_t *p;
	uint8_t pub_buf[1024];
	uint16_t len, i = 0;

	p = pub_buf;

	*p++ = (uint8_t)PUBLISH2_TLV_PAYLOAD;
	*p++ = (uint8_t)((payloadlen >> 8) & 0xff);
	*p++ = (uint8_t)(payloadlen & 0xff);
	memcpy(p, payload, payloadlen);
	p += payloadlen;

	len = strlen(topicName);
	*p++ = (uint8_t)PUBLISH2_TLV_TOPIC;
	*p++ = (uint8_t)((len >> 8) & 0xff);
	*p++ = (uint8_t)(len & 0xff);
	memcpy(p, topicName, len);
	p += len;

	if (opt) {
		uint8_t j = 0;
		int size = cJSON_GetArraySize(opt);
		for (j = 0; j < size; j++) {
			cJSON * test = cJSON_GetArrayItem(opt, j);
			uint8_t i = 0;
			for (i = 0; i < PUBLISH2_TLV_MAX_NUM; i++) {
				if (strcmp(test->string, key[i]) == 0) {
					switch (i) {
					case PUBLISH2_TLV_TTL:
					case PUBLISH2_TLV_TIME_DELAY:
					case PUBLISH2_TLV_QOS:
					{
						*p++ = (uint8_t)i;
						*p++ = 0;
						*p++ = 2;
						memcpy(p, test->valuestring, 2);
						p += 2;
						break;
					}

					case PUBLISH2_TLV_APN_JSON:
					{
						len = strlen(test->valuestring);
						*p++ = (uint8_t)PUBLISH2_TLV_APN_JSON;
						*p++ = (uint8_t)((len >> 8) & 0xff);
						*p++ = (uint8_t)(len & 0xff);
						memcpy(p, test->valuestring, len);
						p += len;
						break;
					}

					default:
						break;
					}
				}
			}
		}
	}

	return MQTTExtendedCmd(c, PUBLISH2, pub_buf, p-pub_buf, DEFAULT_QOS, DEFAULT_RETAINED);
}

int MQTTPublish2ToAlias(MQTTClient* c,
				const char* alias, void* payload, int payloadlen, cJSON *opt)
{
	char buf[150];

	sprintf(buf, ",yta/%s", alias);
	return MQTTPublish2(c, buf, payload, payloadlen, opt);
}


int MQTTClient_get_host(char *appkey, char* url)
{
	int rc = FAILURE;
	char buf[1024];
	char json_data[512];
	Network n;
	int ret;

	sprintf(json_data, "{\"a\":\"%s\",\"n\":\"%s\",\"v\":\"%s\",\"o\":\"%s\"}",
					appkey, /*${networktype}*/"1", "v1.0.0", /*${NetworkOperator}*/"1");

	sprintf(buf,
			"POST %s HTTP/1.1\r\nHost: %s:%d\r\nAccept: application/json\r\nContent-Type: application/json\r\nContent-Length: %d\n\n%s",
			"/", "tick.yunba.io", 9999, strlen(json_data), json_data);

	NetworkInit(&n);
	ret = NetworkConnect(&n, "tick.yunba.io", 9999);
	ret = n.mqttwrite(&n, buf, strlen(buf), 1000);

	if (ret == strlen(buf)) {
		char *temp;
		memset(buf, 0, sizeof(buf));
		ret = n.mqttread(&n, buf, sizeof(buf), 3000);
	//	if (ret > 0) {
			temp = strstr(buf, "\r\n\r\n");
			if (temp) {
				char *p, *q;
				temp += 4;
				p= strstr(temp, ":");
				q = strstr(temp, "}");
				if (p && q) {
					p += 2;
					sprintf(url, "%.*s", q-p-1, p);
					rc = SUCCESS;
				}
	//		}
		}
	}
	n.disconnect(&n);
exit:
	return rc;
}

int MQTTClient_get_host_v2(char *appkey, char* url)
{
	int rc = FAILURE;
	char buf[1024];
	char json_data[512];
	Network n;
	int ret;
	uint16_t json_len;
	uint16_t len;

	sprintf(json_data, "{\"a\":\"%s\",\"n\":\"%s\",\"v\":\"%s\",\"o\":\"%s\"}",
					appkey, /*${networktype}*/"1", "v1.0.0", /*${NetworkOperator}*/"1");

	json_len = strlen(json_data);
	buf[0] = 1; //version
	buf[1] = (uint8_t)((json_len >> 8) & 0xff);
	buf[2] = (uint8_t)(json_len & 0xff);
	len = json_len + 3;
	memcpy(buf + 3, json_data, json_len);

	NetworkInit(&n);
	ret = NetworkConnect(&n, "tick-t.yunba.io", 9977);
	ret = n.mqttwrite(&n, buf, len, 1000);

	if (ret == len) {
		memset(buf, 0, sizeof(buf));
		ret = n.mqttread(&n, buf, sizeof(buf), 3000);
		// packet: version number(1 byte) + json length(2 byte) + json
		//FIXME: condition: ret > 3
		len = (uint16_t)(((uint8_t)buf[1] << 8) | (uint8_t)buf[2]);
		if (len == strlen(buf + 3)) {
			cJSON *root = cJSON_Parse(buf + 3);
			if (root) {
				int ret_size = cJSON_GetArraySize(root);
				if (ret_size >= 1) {
					strcpy(url, cJSON_GetObjectItem(root,"c")->valuestring);
					rc = SUCCESS;
				}
				cJSON_Delete(root);
			}
		}
	}
	n.disconnect(&n);
exit:
	return rc;
}


static int get_reg_info_from_json(char *json, REG_info *info)
{
	int ru = FAILURE, rp = FAILURE, rc = FAILURE, rd = FAILURE;
	char *u, *p, *c, *d;

	u = strstr(json, "\"u\": \"");
	if (u) {
		u += 6;
		sprintf(info->username, "%.*s",19, u);
		ru = SUCCESS;
	}

	p = strstr(json, "\"p\": \"");
	if (p) {
		p += 6;
		sprintf(info->password, "%.*s",13, p);
		rp = SUCCESS;
	}

	c = strstr(json, "\"c\": \"");
	if (c) {
		c += 6;
		sprintf(info->client_id, "%.*s",23, c);
		rc = SUCCESS;
	}

	d = strstr(json, "\"d\": \"");
	if (d) {
		d += 6;
		sprintf(info->device_id, "%.*s",32, d);
		rd = SUCCESS;
	}

	return ((rc == SUCCESS && ru == SUCCESS && rp == SUCCESS && rc == SUCCESS)? SUCCESS : FAILURE);
}

int MQTTClient_setup_with_appkey(char* appkey, REG_info *info)
{
	int rc = FAILURE;
	char buf[1024];
	char json_data[512];
	int ret;
	Network n;

	if (appkey == NULL)
		goto exit;

	sprintf(json_data, "{\"a\": \"%s\", \"p\":4}", appkey);\
	sprintf(buf,
			"POST %s HTTP/1.1\r\nHost: %s:%d\r\nAccept: application/json\r\nContent-Type: application/json\r\nContent-Length: %d\n\n%s",
			"/device/reg/", "reg.yunba.io", 8383, strlen(json_data), json_data);

	NetworkInit(&n);
	ret = NetworkConnect(&n, "reg.yunba.io", 8383);
	ret = n.mqttwrite(&n, buf, strlen(buf), 1000);

	if (ret == strlen(buf)) {
		char *temp;
		memset(buf, 0, sizeof(buf));
		ret = n.mqttread(&n, buf, sizeof(buf), 3000);
	//	if (ret > 0) {
			temp = strstr(buf, "\r\n\r\n");
			if (temp) {
				temp += 4;
				rc = get_reg_info_from_json(temp, info);
			}
//		}
	}
	n.disconnect(&n);
exit:
	return rc;
}

int MQTTClient_setup_with_appkey_v2(char* appkey, REG_info *info)
{
	int rc = FAILURE;
	char buf[1024];
	char json_data[512];
	int ret;
	Network n;
	uint16_t json_len;
	uint16_t len;

	if (appkey == NULL)
		goto exit;

	sprintf(json_data, "{\"a\": \"%s\", \"p\":4}", appkey);

	json_len = strlen(json_data);
	buf[0] = 1; //version
	buf[1] = (uint8_t)((json_len >> 8) & 0xff);
	buf[2] = (uint8_t)(json_len & 0xff);
	len = json_len + 3;
	memcpy(buf + 3, json_data, json_len);

	NetworkInit(&n);
	ret = NetworkConnect(&n, "reg-t.yunba.io", 9944);
	ret = n.mqttwrite(&n, buf, len, 1000);

	if (ret == len) {
		memset(buf, 0, sizeof(buf));
		ret = n.mqttread(&n, buf, sizeof(buf), 3000);
		//FIXME: condition: ret > 3
		len = (uint16_t)(((uint8_t)buf[1] << 8) | (uint8_t)buf[2]);
		if (len == strlen(buf + 3)) {
			cJSON *root = cJSON_Parse(buf + 3);
			if (root) {
				int ret_size = cJSON_GetArraySize(root);
				if (ret_size >= 4) {
					strcpy(info->client_id, cJSON_GetObjectItem(root,"c")->valuestring);
					strcpy(info->username, cJSON_GetObjectItem(root,"u")->valuestring);
					strcpy(info->password, cJSON_GetObjectItem(root,"p")->valuestring);
					strcpy(info->device_id, cJSON_GetObjectItem(root,"d")->valuestring);
					rc = SUCCESS;
				}
				cJSON_Delete(root);
			}
		}
	}
	n.disconnect(&n);
exit:
	return rc;
}

int MQTTClient_setup_with_appkey_and_deviceid(char* appkey, char *deviceid, REG_info *info)
{
	int rc = FAILURE;
	char buf[1024];
	char json_data[512];
	int ret;
	Network n;

	if (appkey == NULL)
		goto exit;

    if (deviceid == NULL)
            sprintf(json_data, "{\"a\": \"%s\", \"p\":4}", appkey);
    else
            sprintf(json_data, "{\"a\": \"%s\", \"p\":4, \"d\": \"%s\"}", appkey, deviceid);

	sprintf(buf,
			"POST %s HTTP/1.1\r\nHost: %s:%d\r\nAccept: application/json\r\nContent-Type: application/json\r\nContent-Length: %d\n\n%s",
			"/device/reg/", "reg.yunba.io", 8383, strlen(json_data), json_data);

	NetworkInit(&n);
	ret = NetworkConnect(&n, "reg.yunba.io", 8383);
	ret = n.mqttwrite(&n, buf, strlen(buf), 1000);

	if (ret == strlen(buf)) {
		char *temp;
		memset(buf, 0, sizeof(buf));
		ret = n.mqttread(&n, buf, sizeof(buf), 3000);
	//	if (ret > 0) {
			temp = strstr(buf, "\r\n\r\n");
			if (temp) {
				temp += 4;
				rc = get_reg_info_from_json(temp, info);
			}
//		}
	}
	n.disconnect(&n);
exit:
	return rc;
}

int MQTTClient_setup_with_appkey_and_deviceid_v2(char* appkey, char *deviceid, REG_info *info)
{
	int rc = FAILURE;
	char buf[1024];
	char json_data[512];
	int ret;
	Network n;
	uint16_t json_len;
	uint16_t len;

	if (appkey == NULL)
		goto exit;

    if (deviceid == NULL)
            sprintf(json_data, "{\"a\": \"%s\", \"p\":4}", appkey);
    else
            sprintf(json_data, "{\"a\": \"%s\", \"p\":4, \"d\": \"%s\"}", appkey, deviceid);

	json_len = strlen(json_data);
	buf[0] = 1; //version
	buf[1] = (uint8_t)((json_len >> 8) & 0xff);
	buf[2] = (uint8_t)(json_len & 0xff);
	len = json_len + 3;
	memcpy(buf + 3, json_data, json_len);

	NetworkInit(&n);
	ret = NetworkConnect(&n, "reg-t.yunba.io", 9944);
	ret = n.mqttwrite(&n, buf, len, 1000);

	if (ret == len) {
		memset(buf, 0, sizeof(buf));
		ret = n.mqttread(&n, buf, sizeof(buf), 3000);
		//FIXME: condition: ret > 3
		len = (uint16_t)(((uint8_t)buf[1] << 8) | (uint8_t)buf[2]);
		if (len == strlen(buf + 3)) {
			cJSON *root = cJSON_Parse(buf + 3);
			if (root) {
				int ret_size = cJSON_GetArraySize(root);
				if (ret_size >= 4) {
					strcpy(info->client_id, cJSON_GetObjectItem(root,"c")->valuestring);
					strcpy(info->username, cJSON_GetObjectItem(root,"u")->valuestring);
					strcpy(info->password, cJSON_GetObjectItem(root,"p")->valuestring);
					strcpy(info->device_id, cJSON_GetObjectItem(root,"d")->valuestring);
					rc = SUCCESS;
				}
				cJSON_Delete(root);
			}
		}
	}
	n.disconnect(&n);
exit:
	return rc;
}
