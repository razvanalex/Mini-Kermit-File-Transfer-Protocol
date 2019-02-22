#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "lib.h"
#include "miniKermit.h"

#define HOST "127.0.0.1"
#define PORT 10000

/* Constants */
#define MAXL        	250
#define TIME        	5
#define MAX_NUM_FAILS	3
#define VERBOSE			1

/* DO NOT change these constants! */
#define _1_SEC_			1000
#define FIRST_SEQ   	0

int sendInitPkt(byte seq) 
{
	msg t;
	
	// Create Send-Init data
	TSYData data = createSYData(MAXL, TIME);

	// Create a new pack and put data in it
	TKermitPkt pkt = createKermitPkt(seq, TYPE_S, 
									(byte*)&data, sizeof(TSYData));

	// Load payload and send the message							
	loadPayload(&t, &pkt);
	return send_message(&t);
}

int sendPkt(byte seq, const char type, byte* data, byte len) 
{
	msg t;

	// Check if length of data is less than MAXL
	if (len > MAXL)
		return E_INVALID;

	// Create a new pack and put data in it
	TKermitPkt pkt = createKermitPkt(seq, type, data, len);

	// Load payload and send the message
	loadPayload(&t, &pkt);
	return send_message(&t);
}

int initConnection(char *host, byte seq, TKermitPkt* pkt, int r_timeout)
{
	int returnCode, fail_counter = 0;

	do {
		// Send Init packet
		verboseSend(host, seq, VERBOSE);
		sendInitPkt(seq);
		
		// Listen for ACK or NAK
		verboseListen(host, VERBOSE);
		returnCode = receivePkt(host, pkt, CREATE_BUF, AUTO_SIZE, r_timeout, 
								VERBOSE);

		// Error during transmission, increase fail counter
		if (returnCode < 0 || pkt->type == TYPE_N)
			fail_counter++;

		// Clear data buffer for
		if (returnCode < 0 && pkt->data != NULL)
			free(pkt->data);

		// Error received is wrong checksum
		if (returnCode == E_CHKSUM && fail_counter <= MAX_NUM_FAILS)
			continue;
		
		// Packet correctly received
		if (returnCode >= 0 && pkt->type == TYPE_Y)
			break;
		
		// For NAK received, increase SEQ
		if (returnCode >= 0 && pkt->type == TYPE_N)
			seq = nextSeq(pkt->seq);

		// Too many fails, connection error returned
		if (fail_counter > MAX_NUM_FAILS)
		{
			fprintf(stderr, "[%s] Too many fails! Abort!\n", host);
			return E_CONN;
		}
	} while (1);

	// Return next SEQ for success, or E_NULL error otherwise.
	if(pkt->type == TYPE_Y)
		return nextSeq(pkt->seq);
	return E_NULL;
}

int checkSeq(byte lastSeq, byte newSeq) 
{
	if (nextSeq(lastSeq) == 0 || nextSeq(lastSeq) == 1)
		return nextSeq(lastSeq) >= newSeq;
	return nextSeq(lastSeq) <= newSeq;
}

int sendPktWithCheck(char* host, TKermitPkt* recPkt, int r_timeout, 
					 int r_bufSize, byte* seq, const char type, byte* data, 
					 byte len)
{
	int returnCode, fail_counter = 0;

	do {
		// Send packet
		verboseSend(host, *seq, VERBOSE);
		sendPkt(*seq, type, data, len);
	
		// Listen for ACK or NAK
		verboseListen(host, VERBOSE);	
		if (recPkt->data == NULL) 
			returnCode = receivePkt(host, recPkt, CREATE_BUF, r_bufSize, 
									r_timeout, VERBOSE);
		else 
			returnCode = receivePkt(host, recPkt, OLD_BUF, r_bufSize, 
									r_timeout, VERBOSE);

		// Error during transmission
		if (returnCode < 0 || recPkt->type == TYPE_N) 
			fail_counter++;

		// Check sequence number
		if (returnCode >= 0 && !checkSeq(*seq, recPkt->seq) 
				&& fail_counter <= MAX_NUM_FAILS) {
			verboseString(host, "Send again", VERBOSE);
			continue;
		}

		//If ACK or NAK has wrong checkkSum, drop it and send the same packet
		if (returnCode == E_CHKSUM && fail_counter <= MAX_NUM_FAILS)
			continue;

		// Check for NAK or ACK
		if (returnCode >= 0 && checkSeq(*seq, recPkt->seq)
				&& fail_counter <= MAX_NUM_FAILS) 
		{
			if (recPkt->type == TYPE_N) 		// NAK received
				*seq = nextSeq(recPkt->seq); 
			else if (recPkt->type == TYPE_Y) 	// ACK received
				break;
		}

		// Too many fails, return E_CONN
		if (fail_counter > MAX_NUM_FAILS)
		{
			fprintf(stderr, "[%s] Too many fails! Abort!\n", host);
			return E_CONN;
		}
	} while (1);

	// Return SUCCESS, or E_NULL error otherwise, and update SEQ to next one.
	if(recPkt->type == TYPE_Y) 
	{
		*seq = nextSeq(recPkt->seq);
		return SUCCESS;
	}
	return E_NULL;
}

int sendFile(char* host, TKermitPkt* pkt, char* filename, byte* seq,
			 int r_timeout, int r_bufSize)
{
	char *buf;
	int len, crt_size = 0;
	struct stat st;

	// Get file statistics (size needed for verbose)
	stat(filename, &st);

	// Open file in read mode
	int fd = open(filename, O_RDONLY);
	if (fd < 0) {
		perror("Error");
		return -1;
	}
	
	// Create buffer
	buf = (char*)malloc(MAXL * sizeof(char));
	if (!buf)
		return -1;

	// Send file header
	verboseString(host, "-------- FILEHEADER --------", VERBOSE);
	if (sendPktWithCheck(host, pkt, r_timeout, r_bufSize, seq, TYPE_F, 
						(byte*)filename, strlen(filename) + 1) < 0)
		return -1;

	// Send file data
	while ((len = read(fd, buf, MAXL)) > 0) 
	{
		crt_size += len;

		if (VERBOSE)
			printf("[%s] -------- SENDING %d/%lu -------- \n", host, crt_size, 
				   st.st_size);

		if (sendPktWithCheck(host, pkt, r_timeout, r_bufSize, seq, TYPE_D, 
							(byte*)buf, len) < 0) {
			return -1;
		}
	}
	verboseString(host, "-------- EOF --------", VERBOSE);

	// Send EOF packet
	if (sendPktWithCheck(host, pkt, r_timeout, r_bufSize, seq, TYPE_Z, 
						 NULL, 0) < 0) 
		return -1;
		
	verboseString(host, "-------- DONE --------", VERBOSE);

	// Close file and free buf
	close(fd);
	free(buf);

	return SUCCESS;
}

int main(int argc, char** argv) 
{
	TKermitPkt pkt;
	byte seq = FIRST_SEQ;
	int r_timeout = TIME * _1_SEC_;
	int r_bufSize = 0;

	// Init sender
	init(HOST, PORT);

	// Init connection (send SendInit packet)
	if ((char)(seq = initConnection(argv[0], seq, &pkt, r_timeout)) < 0)
		return -1;

	// Set timeout and buffer size
	r_timeout = ((TSYData*)pkt.data)->time * _1_SEC_;
	r_bufSize = ((TSYData*)pkt.data)->maxl;
	free(pkt.data);
	
	// Send each file
	for (int i = 1; i < argc; i++) {
		printf("[%s] -------- SEND %s -------- \n", argv[0], argv[i]);
		if(sendFile(argv[0], &pkt, argv[i], &seq, r_timeout, r_bufSize) < 0)
			return -1;
	}

	verboseString(argv[0], "-------- END TRANSMISSION --------", VERBOSE);
	
	// Send End of transmission packet
	if (sendPktWithCheck(argv[0], &pkt, r_timeout, r_bufSize, &seq, 
						 TYPE_B, NULL, 0) < 0)
		return -1;

	return 0;
}