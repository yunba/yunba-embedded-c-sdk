/*
 * MQTTSerializePublish2.c
 *
 *  Created on: May 5, 2015
 *      Author: yunba
 */
#include "MQTTPacket.h"
#include "StackTrace.h"

#include <string.h>


int MQTTSerialize_extendedcmdLength(int qos, EXTED_CMD cmd, void *param, int param_len)
{
	int len = 0;

	/* <packetid> + cmd(1byte) + len(the length of param)*/
	len += 1 + param_len + 2;
	if (qos > 0)
		len += 8; /* packetid */
	return len;
}


int MQTTSerialize_extendedcmd(unsigned char* buf, int buflen, unsigned char dup, int qos, unsigned char retained, uint64_t packetid,
		EXTED_CMD cmd, void *payload, int payloadlen)
{
	unsigned char *ptr = buf;
	MQTTHeader header = {0};
	int rem_len = 0;
	int rc = 0;

	FUNC_ENTRY;
	if (MQTTPacket_len(rem_len = MQTTSerialize_extendedcmdLength(qos, cmd, payload, payloadlen)) > buflen)
	{
		rc = MQTTPACKET_BUFFER_TOO_SHORT;
		goto exit;
	}
//	printf("%s, %i\n", __func__, rem_len);

	header.bits.type = EXTCMD;
	header.bits.dup = dup;
	header.bits.qos = qos;
	header.bits.retain = retained;
	writeChar(&ptr, header.byte); /* write header */

	ptr += MQTTPacket_encode(ptr, rem_len); /* write remaining length */;

	if (qos > 0)
		writeInt64(&ptr, packetid);

	writeChar(&ptr, cmd);
	writeInt(&ptr, payloadlen);

	memcpy(ptr, payload, payloadlen);
	ptr += payloadlen;

	rc = ptr - buf;

exit:
	FUNC_EXIT_RC(rc);
	return rc;
}
