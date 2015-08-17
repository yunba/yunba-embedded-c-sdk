/*******************************************************************************
 * Copyright (c) 2012, 2013 IBM Corp.
 *
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * and Eclipse Distribution License v1.0 which accompany this distribution. 
 *
 * The Eclipse Public License is available at 
 *   http://www.eclipse.org/legal/epl-v10.html
 * and the Eclipse Distribution License is available at 
 *   http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * Contributors:
 *    Ian Craggs - initial contribution
 *    Ian Craggs - change delimiter option from char to string
 *    Al Stockdill-Mander - Version using the embedded C client
 *******************************************************************************/

/*
 
 stdout subscriber
 
 compulsory parameters:
 
  topic to subscribe to
 
 defaulted parameters:
 
	--host localhost
	--port 1883
	--qos 2
	--delimiter \n
	--clientid stdout_subscriber
	
	--userid none
	--password none

 for example:

    stdoutsub topic/of/interest --host iot.eclipse.org

*/
#include <stdio.h>
#include "MQTTClient.h"

#include <stdio.h>
#include <signal.h>
#include <memory.h>

#include <sys/time.h>


volatile int toStop = 0;
static void messageArrived(MessageData* md);


void usage()
{
	printf("MQTT stdout subscriber\n");
	printf("Usage: stdoutsub topicname <options>, where options are:\n");
	printf("  --host <hostname> (default is localhost)\n");
	printf("  --port <port> (default is 1883)\n");
	printf("  --qos <qos> (default is 2)\n");
	printf("  --delimiter <delim> (default is \\n)\n");
	printf("  --clientid <clientid> (default is hostname+timestamp)\n");
	printf("  --username none\n");
	printf("  --password none\n");
	printf("  --showtopics <on or off> (default is on if the topic has a wildcard, else off)\n");
	exit(-1);
}


void cfinish(int sig)
{
	signal(SIGINT, NULL);
	toStop = 1;
}


struct opts_struct
{
	char* clientid;
	int nodelimiter;
	char* delimiter;
	enum QoS qos;
	char* username;
	char* password;
	char* host;
	int port;
	int showtopics;
	char *appkey;
	char *deviceid;
} opts =
{
	(char*)"stdout-subscriber", 0, (char*)"\n", QOS2, NULL, NULL, (char*)"localhost", 1883, 0
};


void getopts(int argc, char** argv)
{
	int count = 2;
	
	while (count < argc)
	{
		if (strcmp(argv[count], "--qos") == 0)
		{
			if (++count < argc)
			{
				if (strcmp(argv[count], "0") == 0)
					opts.qos = QOS0;
				else if (strcmp(argv[count], "1") == 0)
					opts.qos = QOS1;
				else if (strcmp(argv[count], "2") == 0)
					opts.qos = QOS2;
				else
					usage();
			}
			else
				usage();
		}
		else if (strcmp(argv[count], "--host") == 0)
		{
			if (++count < argc)
				opts.host = argv[count];
			else
				usage();
		}
		else if (strcmp(argv[count], "--port") == 0)
		{
			if (++count < argc)
				opts.port = atoi(argv[count]);
			else
				usage();
		}
		else if (strcmp(argv[count], "--clientid") == 0)
		{
			if (++count < argc)
				opts.clientid = argv[count];
			else
				usage();
		}
		else if (strcmp(argv[count], "--username") == 0)
		{
			if (++count < argc)
				opts.username = argv[count];
			else
				usage();
		}
		else if (strcmp(argv[count], "--password") == 0)
		{
			if (++count < argc)
				opts.password = argv[count];
			else
				usage();
		}
		else if (strcmp(argv[count], "--delimiter") == 0)
		{
			if (++count < argc)
				opts.delimiter = argv[count];
			else
				opts.nodelimiter = 1;
		}
		else if (strcmp(argv[count], "--showtopics") == 0)
		{
			if (++count < argc)
			{
				if (strcmp(argv[count], "on") == 0)
					opts.showtopics = 1;
				else if (strcmp(argv[count], "off") == 0)
					opts.showtopics = 0;
				else
					usage();
			}
			else
				usage();
		}
		else if (strcmp(argv[count], "--appkey") == 0)
		{
			if (++count < argc)
				opts.appkey = argv[count];
			else
				usage();
		}
		else if (strcmp(argv[count], "--deviceid") == 0)
		{
			if (++count < argc)
				opts.deviceid = argv[count];
			else
				usage();
		}
		count++;
	}
}


static void messageArrived(MessageData* md)
{
	printf("%s, ", __func__);
	MQTTMessage* message = md->message;

//	if (opts.showtopics)
		printf("topic: %.*s, ", md->topicName->lenstring.len, md->topicName->lenstring.data);
//	if (opts.nodelimiter)
		printf("Message: %.*s\n", (int)message->payloadlen, (char*)message->payload);
//	else
//		printf("%.*s%s", (int)message->payloadlen, (char*)message->payload, opts.delimiter);
	//fflush(stdout);
}

static void extMessageArrive(EXTED_CMD cmd, int status, int ret_string_len, char *ret_string)
{
	printf("%s, cmd:%d, status:%d, payload: %.*s\n", __func__, cmd, status, ret_string_len, ret_string);
}

static int get_ip_pair(const char *url, char *addr, int *port)
{
	char *p = strstr(url, "tcp://");
	if (p) {
		p += 6;
		char *q = strstr(p, ":");
		if (q) {
			int len = strlen(p) - strlen(q);
			if (len > 0) {
				sprintf(addr, "%.*s", len, p);
				*port = atoi(q + 1);
				return SUCCESS;
			}
		}
	}
	return FAILURE;
}

int main(int argc, char** argv)
{
	int rc = 0;
	unsigned char buf[100];
	unsigned char readbuf[100];
	
	if (argc < 2)
		usage();
	
	char* topic = argv[1];

	if (strchr(topic, '#') || strchr(topic, '+'))
		opts.showtopics = 1;
	if (opts.showtopics)
		printf("topic is %s\n", topic);

	getopts(argc, argv);	

	Network n;
	Client c;

	signal(SIGINT, cfinish);
	signal(SIGTERM, cfinish);

	REG_info reg;
	rc = MQTTClient_setup_with_appkey_and_deviceid(opts.appkey, opts.deviceid, &reg);
	printf("get reg info: client-id:%s, username:%s, password:%s, deviceid:%s\n",
			reg.client_id, reg.username, reg.password, reg.device_id);

	char url[50];
	MQTTClient_get_host(opts.appkey, url);
	printf("get broker: %s\n", url);

	NewNetwork(&n);

	char ip[100];
	int port = 1883;
	get_ip_pair(url, ip, &port);
	ConnectNetwork(&n, /*opts.host*/ip, /*opts.port*/port);
	MQTTClient(&c, &n, 1000, buf, 100, readbuf, 100);
 
	MQTTPacket_connectData data = MQTTPacket_connectData_initializer;       
	data.willFlag = 0;
	data.MQTTVersion = 19;
	data.clientID.cstring = /*opts.clientid*/reg.client_id;
	data.username.cstring = /*opts.username*/reg.username;
	data.password.cstring = /*opts.password*/reg.password;

	data.keepAliveInterval = 100;
	data.cleansession = 0;

	printf("Connecting to %s %d\n", /*opts.host*/ip, /*opts.port*/port);
	
	rc = MQTTConnect(&c, &data);
	printf("Connected %d\n", rc);
    
	MQTTSetCallBack(&c, messageArrived, extMessageArrive);

//    rc = MQTTUnsubscribe(&c, "hello");

	rc = MQTTSubscribe(&c, topic, QOS1);
	printf("Subscribed %d\n", rc);

	MQTTMessage M;
	M.qos = 1;
	char temp[100];
	strcpy(temp, "Helow World\n");
	M.payload = temp;
	M.id = 1000;
	M.payloadlen = strlen(temp);
	rc = MQTTPublish(&c, topic, &M);
	printf("published %d\n", rc);

	rc = MQTTSetAlias(&c, "Jerry");
	printf("set alias %d\n", rc);

	rc = MQTTPublishToAlias(&c, "Jerry", "Hello", strlen("Hello"));
	printf("publish to alias %d\n", rc);

	rc = MQTTGetAlias(&c, "unknow");
	printf("get alias %d\n", rc);

	rc = MQTTGetStatus(&c, "Jerry");
	printf("get status %d\n", rc);

	rc = MQTTGetAliasList(&c, topic);
	printf("alias list get %d\n", rc);

	rc = MQTTGetTopic(&c, "Jerry");
	printf("get topic %d\n", rc);


	while (!toStop)
	{
		MQTTYield(&c, 1000);
	}
	
	printf("Stopping\n");

	MQTTDisconnect(&c);
	n.disconnect(&n);

	return 0;
}


