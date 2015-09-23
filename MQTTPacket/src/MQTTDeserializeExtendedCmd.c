/*
 * MQTTDeserializePublish2.c
 *
 *  Created on: May 5, 2015
 *      Author: yunba
 */
#include "StackTrace.h"
#include "MQTTPacket.h"
#include <string.h>

int MQTTDeserialize_extendedcmd(unsigned char* dup, int* qos, unsigned char* retained, uint64_t* packetid,
		EXTED_CMD* cmd, int *status,
		void** payload, int* payloadlen, unsigned char* buf, int buflen)
{
	MQTTHeader header = {0};
	unsigned char* curdata = buf;
	unsigned char* enddata = NULL;
	int rc = 0;
	int mylen = 0;

	FUNC_ENTRY;
	header.byte = readChar(&curdata);
	if (header.bits.type != EXTCMD)
		goto exit;
	*dup = header.bits.dup;
	*qos = header.bits.qos;
	*retained = header.bits.retain;

	curdata += (rc = MQTTPacket_decodeBuf(curdata, &mylen)); /* read remaining length */
	enddata = curdata + mylen;

//	printf("%s, retain len:%d, qos:%d\n", __func__, mylen, *qos);

//	if (*qos > 0)
		*packetid = readInt64(&curdata);

	*cmd = (*curdata);
//	printf("%s, cmd: %d\n", __func__, *cmd);
	curdata++;
	*status = (*curdata);
//	printf("%s, status: %d\n", __func__, *status);
	curdata++;
	*payloadlen = readInt(&curdata);;
//	printf("%s, payload len: %d\n", __func__, *payloadlen);
	*payload = curdata;
//	printf("%s, payload: %s\n", __func__, *payload);
	rc = 1;
exit:
	FUNC_EXIT_RC(rc);
	return rc;
}
