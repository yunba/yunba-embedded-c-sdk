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
 *    Ian Craggs - initial API and implementation and/or initial documentation
 *    Xiang Rong - 442039 Add makefile to Embedded C client
 *******************************************************************************/

#ifndef MQTTEXTENDEDCMD_H_
#define MQTTEXTENDEDCMD_H_

#if !defined(DLLImport)
  #define DLLImport
#endif
#if !defined(DLLExport)
  #define DLLExport
#endif

DLLExport int MQTTSerialize_extendedcmd(unsigned char* buf, int buflen, unsigned char dup, int qos, unsigned char retained, uint64_t packetid,
		EXTED_CMD cmd, void *payload, int payloadlen);

DLLExport int MQTTDeserialize_extendedcmd(unsigned char* dup, int* qos, unsigned char* retained, uint64_t* packetid,
		EXTED_CMD* cmd, int *status,
		void** payload, int* payloadlen, unsigned char* buf, int buflen);

#endif /* MQTTEXTENDEDCMD_H_ */
