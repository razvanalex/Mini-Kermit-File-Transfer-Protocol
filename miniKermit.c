#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "lib.h"
#include "miniKermit.h"

TKermitPkt createKermitPkt(byte seq, const char type, byte* data, byte dataLen)
{
	TKermitPkt initPkt;

	initPkt.soh = 0x01;

	// Substract SOH and LEN fields, the length of 
	// the pointer to data and replace it with its actual size
	initPkt.len = sizeof(TKermitPkt) - 2 - sizeof(byte*) + dataLen;
 
	initPkt.seq = seq;
	initPkt.type = type;
	initPkt.data = data;
	initPkt.check = 0x0;
	initPkt.mark = 0xD;

	return initPkt;
}

TSYData createSYData(byte maxl, byte time)
{
	TSYData initData;

	memset(&initData, 0, sizeof(TSYData));
	initData.maxl = maxl;
	initData.time = time;
	initData.npad = 0x0;
	initData.padc = 0x0;
	initData.eol = 0xD;

	return initData;
}

void packData(msg* msgAddr, void* data, int length) 
{
	if (!msgAddr)
		return;

	msgAddr->len = length;
	memset(&msgAddr->payload, 0, PAYLOAD_LEN);
	memcpy(&msgAddr->payload, data, length);
}

void loadPayload(msg* message, TKermitPkt* pkt) 
{
	const byte headerSize = 4 * sizeof(byte);
	const byte dataLen = pkt->len + 2 + sizeof(byte*) - sizeof(TKermitPkt); 
	const byte sizeFooter = sizeof(unsigned short) + sizeof(byte);
	const short payloadLen = headerSize + dataLen + sizeFooter;

	char payload[PAYLOAD_LEN];
	
	// Init payload to 0
	memset(&payload, 0, PAYLOAD_LEN);

	// Copy packet to payload
	memcpy(payload, pkt, headerSize);
	memcpy(&payload[headerSize], pkt->data, dataLen);

	// Compute CRC, and put the rest in payload
	pkt->check = crc16_ccitt(payload, headerSize + dataLen);
	memcpy(&payload[headerSize + dataLen], &pkt->check, sizeFooter);

	// Copy data to message
	packData(message, payload, payloadLen);
}

int extractPayload(msg message, TKermitPkt* pkt, int newBuffer, int bufMaxSize) 
{
	if (!pkt)
		return E_NULL;
	
	if (message.len <= 0)
		return E_INVALID;

	byte headerSize, dataLen, chkSize, footerSize;

	// Extract header
	headerSize = 4 * sizeof(byte);
	memcpy(pkt, &message.payload, headerSize);

	// Extract footer from packet
	dataLen = pkt->len + 2 * sizeof(byte) + sizeof(byte*) - sizeof(TKermitPkt);
	chkSize = headerSize + dataLen;
	footerSize = sizeof(unsigned short) + sizeof(byte);
	memcpy(&pkt->check, &message.payload[chkSize], footerSize);
	
	// Check sum
	unsigned short crc16 = crc16_ccitt(&message.payload, headerSize + dataLen);
	if (crc16 != pkt->check) 
	{
		pkt->data = NULL;
		return E_CHKSUM;
	}

	// Extract data from packet
	if (dataLen == 0) 
	{
		pkt->data = NULL;
		return SUCCESS;
	}
	
	if (newBuffer) 
	{
		byte bufSize = bufMaxSize >= 0 ? bufMaxSize : dataLen;
		byte* data = (byte*)malloc(bufSize * sizeof(byte));
		if (!data)
			return E_NULL;

		memcpy(data, &message.payload[headerSize], dataLen * sizeof(byte));
		pkt->data = data;
	}
	else  {
		memcpy(pkt->data, &message.payload[headerSize], dataLen * sizeof(byte));
	}

	return SUCCESS;
}

int receivePkt(char* host, TKermitPkt* recvPkt, int newBuffer, int bufMaxSize, 
	int timeout, int verbose) 
{
	msg *y = receive_message_timeout(timeout);

	if (y == NULL) 
	{
		memset(recvPkt, 0, sizeof(TKermitPkt));
		fprintf(stderr, "[%s] Timeout!\n", host);
		return E_TIMEOUT;
	}
	else 
	{
		int returnCode = extractPayload(*y, recvPkt, newBuffer, bufMaxSize);
		if (returnCode < 0)  
		{	
			fprintf(stderr, "[%s] Error while extracting...\n", host);		
			return returnCode;
		}
		if (verbose != 0) 
			verbosePkt(host, recvPkt);
	}
	return SUCCESS;
}

void verbosePkt(char* host, TKermitPkt* recvPkt) 
{
	if (!recvPkt || !host)
		return;

	switch (recvPkt->type)
	{
		case TYPE_S: 
			printf("[%s] Got S-type packet: SOH=%x LEN=%x SEQ=%d CHECK=%x "
				"MARK=%x\n", host, recvPkt->soh & 0xff, 
				recvPkt->len & 0xff, recvPkt->seq & 0xff,
				recvPkt->check & 0xffff, recvPkt->mark & 0xff);
			break;
		case TYPE_F: 
			printf("[%s] Got F-type packet: SEQ=%d DATA=%s\n", host, 
				recvPkt->seq & 0xff, recvPkt->data);
			break;
		case TYPE_D: 
			printf("[%s] Got D-type packet: SEQ=%d\n", host, 
				recvPkt->seq & 0xff);
			break;
		case TYPE_Z: 
			printf("[%s] Got Z-type packet: SEQ=%d\n", host, 
				recvPkt->seq & 0xff);
			break;
		case TYPE_B: 
			printf("[%s] Got B-type packet: SEQ=%d\n", host, 
				recvPkt->seq & 0xff);
			break;
		case TYPE_Y:
			if (recvPkt->data == NULL)
			{
				printf("[%s] Got Y-type packet: SEQ=%d\n", host, 
					recvPkt->seq & 0xff);
			}
			else 
			{
				printf("[%s] Got Y-type packet: SOH=%x LEN=%x SEQ=%d "
					"CHECK=%x MARK=%x\n", host, recvPkt->soh & 0xff, 
					recvPkt->len & 0xff, recvPkt->seq & 0xff,
					recvPkt->check & 0xffff, recvPkt->mark & 0xff);
			}
			break;
		case TYPE_N: 
			printf("[%s] Got N-type packet: SEQ=%d\n", host, 
				recvPkt->seq & 0xff);
			break;
		case TYPE_E: 
			printf("[%s] Got E-type packet: SEQ=%d\n", host, 
				recvPkt->seq & 0xff);
			break;
		default:
			printf("[%s] Got unknonw reply\n", host);
	}
}

byte nextSeq(byte seq)
{
	seq = (seq % MAX_SEQ + 1) % MAX_SEQ;
	return seq;
}

void verboseString(char* host, char* str, int active) 
{
	if(!active)
		return;
	
	printf("[%s] %s\n", host, str);
}

void verboseSend(char* host, int seq, int active) 
{
	if(!active)
		return;
	
	if (seq == NO_SEQ)
		printf("[%s] Send packet...\n", host);
	else 
		printf("[%s] Send packet with SEQ=%d...\n", host, seq & 0xff);
}

void verboseListen(char* host, int active) 
{
	if(!active)
		return;
	
	printf("[%s] Host is waiting...\n", host);
}

void verboseACK(char* host, int seq, int active) 
{
	if(!active)
		return;
	
	printf("[%s] Send ACK with SEQ=%d...\n", host, seq & 0xff);
}

void verboseNAK(char* host, int seq, int active) 
{
	if(!active)
		return;
	
	printf("[%s] Send NAK with SEQ=%d...\n", host, seq & 0xff);
}