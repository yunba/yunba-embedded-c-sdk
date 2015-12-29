/* Standard includes. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#define MQTT_TASK 1
#include "MQTTClient.h"

#define YUNBA_MQTT_VER   19

USER_PARM_t parm;
MQTTClient client;
Network network;
MQTTPacket_connectData connectData = MQTTPacket_connectData_initializer;

char *addr;
uint16_t port;

const portTickType xDelay = 1000 / portTICK_RATE_MS;

struct MSG_t {
	uint8_t payload[64];
	uint8_t len;
};

typedef enum {
	ST_INIT, ST_CONNECT, ST_REG, ST_SUB, ST_PUBLISH, ST_RUNNING, ST_DIS, ST_RECONN
} MQTT_STATE_t;

MQTT_STATE_t MQTT_State = ST_INIT;

typedef struct {
        char *appkey;
        char *deviceid;
        char *alias;
        char *topic;
        unsigned short aliveinterval;
} USER_PARM_t;


static int8_t init_user_parm(USER_PARM_t *user_parm)
{
	uint8_t ret = 0;
	user_parm->appkey = (char *)malloc(54);
	user_parm->deviceid = (char *)malloc(54);
	user_parm->topic = (char *)malloc(54);
	user_parm->alias = (char *)malloc(54);

	if (user_parm->appkey == NULL ||
			user_parm->deviceid == NULL ||
			user_parm->topic == NULL ||
			user_parm->alias == NULL) {
		if (user_parm->appkey == NULL) free(user_parm->appkey);
		if (user_parm->deviceid == NULL) free(user_parm->deviceid);
		if (user_parm->topic == NULL) free(user_parm->topic);
		if (user_parm->alias == NULL) free(user_parm->alias);
		ret = -1;
	}
	return ret;
}


static void messageArrived(MessageData* data)
{
	printf("Message arrived on topic %.*s: %.*s\n", data->topicName->lenstring.len, data->topicName->lenstring.data,
		data->message->payloadlen, data->message->payload);
}

static void extMessageArrive(EXTED_CMD cmd, int status, int ret_string_len, char *ret_string)
{

}

static void mqttConnectLost(char *reaseon)
{
	int rc;
	MQTTDisconnect(&client);
	network.disconnect(&network);
	if ((rc = NetworkConnect(&network, addr, port)) != 0)
		printf("Return code from network connect is %d\n", rc);
	else {
		printf("net connect: %d\n", rc);
		rc = MQTTConnect(&client, &connectData, true);
	}
	printf("mqtt connect lost, reconnect:%d\n", rc);
}


static int get_ip_pair(const char *url, char *addr, uint16_t *port)
{
	char *p = strstr(url, "tcp://");
	if (p) {
		p += 6;
		char *q = strstr(p, ":");
		if (q) {
			int len = strlen(p) - strlen(q);
			if (len > 0) {
				memcpy(addr, p, len);
				//sprintf(addr, "%.*s", len, p);
				*port = atoi(q + 1);
				return SUCCESS;
			}
		}
	}
	return FAILURE;
}

static void yunba_get_mqtt_broker(char *appkey, char *deviceid, char *broker_addr, uint16_t *port, REG_info *reg)
{
	char *url = (char *)malloc(128);

	if (url) {
		memset(url, 0, 128);
		do {
			if (MQTTClient_get_host_v2(appkey, url) == SUCCESS)
				break;
			vTaskDelay(30 / portTICK_RATE_MS);
		} while (1);
		printf("get url: %s\n", url);
		get_ip_pair(url, broker_addr, port);
		free(url);
	}

	do {
		if (MQTTClient_setup_with_appkey_v2(appkey, deviceid, reg) == SUCCESS)
			break;
		vTaskDelay(30 / portTICK_RATE_MS);
	} while (1);
}

static void prvMQTTYunbaDemoTask(void *pvParameters)
{
	xQueueHandle QueueMQTTClient = NULL;
	MQTTClient client;
	Network network;
	unsigned char sendbuf[80], readbuf[80];
	int rc = 0, 
		count = 0;
	MQTTPacket_connectData connectData = MQTTPacket_connectData_initializer;

	if (init_user_parm(&parm) != 0) return;

	strcpy(parm.appkey, "<your-appkey>");
	strcpy(parm.deviceid, "<your-devid>");
	strcpy(parm.topic, "<your-topic>");
	strcpy(parm.alias, "<your-alias>");
	parm.aliveinterval = 90;

	if (QueueMQTTClient == NULL)
		QueueMQTTClient = xQueueCreate(5, sizeof(struct MSG_t));

	pvParameters = 0;
	NetworkInit(&network);
	MQTTClientInit(&client, &network, 30000, sendbuf, sizeof(sendbuf), readbuf, sizeof(readbuf));

	REG_info reg;
	reg.client_id = (char *)malloc(56);
	reg.device_id = (char *)malloc(56);
	reg.password = (char *)malloc(56);
	reg.username = (char *)malloc(56);
	addr = (char *)malloc(28);
	memset(addr, 0, 28);
	yunba_get_mqtt_broker(parm.appkey, parm.deviceid, addr, &port, &reg);

	printf("get mqtt broker->%s:%d\n", addr, port);
	printf("get reg info: cid:%s, username:%d, password:%s, devid:%s\n",
			reg.client_id, reg.username, reg.password, reg.device_id);

	MQTTSetCallBack(&client, messageArrived, extMessageArrive, mqttConnectLost);

	while (MQTT_State != ST_RUNNING) {
		switch (MQTT_State) {
		case ST_INIT:
			if ((rc = NetworkConnect(&network, addr, port)) != 0)
				printf("Return code from network connect is %d\n", rc);
			else
				printf("net connect: %d\n", rc);
			MQTT_State = ST_CONNECT;
			break;

		case ST_CONNECT:
			connectData.MQTTVersion = YUNBA_MQTT_VER;
			connectData.clientID.cstring = reg.client_id;
			connectData.username.cstring = reg.username;
			connectData.password.cstring = reg.password;
			connectData.keepAliveInterval = parm.aliveinterval;

			if ((rc = MQTTConnect(&client, &connectData)) != 0) {
				printf("Return code from MQTT connect is %d\n", rc);
			}
			else {
				printf("MQTT Connected\n");
			#if defined(MQTT_TASK)
				if ((rc = MQTTStartTask(&client)) != pdPASS)
					printf("Return code from start tasks is %d\n", rc);
			#endif
				MQTT_State = ST_REG;
			}
			break;

		case ST_REG:
			if ((rc = MQTTSubscribe(&client, parm.topic, QOS2, messageArrived)) != 0)
				printf("Return code from MQTT subscribe is %d\n", rc);
			else
				printf("subscribe: %d\n", rc);
			MQTT_State = ST_SUB;
			break;

		case ST_SUB:
			MQTTSetAlias(&client, parm.alias);
			MQTT_State = ST_RUNNING;
			break;

		default:
			break;
		}
		vTaskDelay(xDelay);
	}

    for (;;) {
		if(xQueueReceive(QueueMQTTClient, &M, 100/portTICK_RATE_MS ) == pdPASS) {
			if ((rc = MQTTPublish2(&client, parm.topic, M.payload, M.len, Opt)) != 0)
				printf("Return code from MQTT publish is %d\n", rc);
		}
	}

	free(reg.client_id);
	free(reg.device_id);
	free(reg.password);
	free(reg.username);
	free(addr);

	vQueueDelete(QueueMQTTClient);
	vTaskDelete(NULL);
}


void vStartMQTTTasks(uint16_t usTaskStackSize, UBaseType_t uxTaskPriority)
{
	BaseType_t x = 0L;

	xTaskCreate(prvMQTTYunbaDemoTask,	/* The function that implements the task. */
			"MQTT_YUNBA",			/* Just a text name for the task to aid debugging. */
			usTaskStackSize,	/* The stack size is defined in FreeRTOSIPConfig.h. */
			(void *)x,		/* The task parameter, not used in this case. */
			uxTaskPriority,		/* The priority assigned to the task is defined in FreeRTOSConfig.h. */
			NULL);				/* The task handle is not used. */
}
/*-----------------------------------------------------------*/


