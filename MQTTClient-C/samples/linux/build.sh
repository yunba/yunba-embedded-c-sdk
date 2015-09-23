rm stdoutsub
cp ../../src/MQTTClient.c .
cp ../../src/cJSON.c .
cp ../../src/cJSON.h .
sed -e 's/""/"MQTTLinux.h"/g' ../../src/MQTTClient.h > MQTTClient.h
gcc stdoutsub.c -g -I ../../src -I ../../src/linux -I ../../../MQTTPacket/src MQTTClient.c cJSON.c ../../src/linux/MQTTLinux.c ../../../MQTTPacket/src/MQTTFormat.c  ../../../MQTTPacket/src/MQTTPacket.c ../../../MQTTPacket/src/MQTTDeserializePublish.c ../../../MQTTPacket/src/MQTTConnectClient.c ../../../MQTTPacket/src/MQTTSubscribeClient.c ../../../MQTTPacket/src/MQTTSerializePublish.c -o stdoutsub ../../../MQTTPacket/src/MQTTConnectServer.c ../../../MQTTPacket/src/MQTTSubscribeServer.c ../../../MQTTPacket/src/MQTTUnsubscribeServer.c ../../../MQTTPacket/src/MQTTUnsubscribeClient.c ../../../MQTTPacket/src/MQTTSerializeExtendedCmd.c ../../../MQTTPacket/src/MQTTDeserializeExtendedCmd.c -lm
