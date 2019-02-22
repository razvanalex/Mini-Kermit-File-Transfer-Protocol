#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include "lib.h"
#include "miniKermit.h"

#define HOST "127.0.0.1"
#define PORT 10001

/* Constants */
#define MAXL        	250
#define TIME        	5
#define MAX_NUM_FAILS	3
#define VERBOSE			1

/* DO NOT change these constants! */
#define _1_SEC_			1000
#define END_TRANS 		-2
#define END_FILE		-3

int sendFirstACK(byte seq) 
{
    msg t;
	
	// Create Send-Init data
	TSYData data = createSYData(MAXL, TIME);

	// Create a new pack and put data in it
	TKermitPkt pkt = createKermitPkt(seq, TYPE_Y, 
									(byte*)&data, sizeof(TSYData));

	// Load payload and send the message
	loadPayload(&t, &pkt);
	return send_message(&t);
}

int sendEmptyData(byte seq, const char type) 
{
    msg t;

	// Create a new pack and put data in it
	TKermitPkt pkt = createKermitPkt(seq, type, NULL, 0);

	// Load payload and send the message
	loadPayload(&t, &pkt);
	return send_message(&t);
}

int sendACK(byte seq) 
{
	return sendEmptyData(seq, TYPE_Y);
}

int sendNAK(byte seq) 
{
	return sendEmptyData(seq, TYPE_N);
}

int createFile(void* name, int nameLen) 
{
	const char* pref = "recv_";

	// Alloc memory for the new name
	char *filename = (char*)malloc(100 * sizeof(char));
	if (!filename)
		return -1;

	// The name of new file
	strncpy(filename, pref, strlen(pref));
	memcpy(filename + strlen(pref), name, nameLen);
	filename[nameLen + strlen(pref)] = '\0';
	
	// Open file
	int fd = open(filename, O_CREAT | O_WRONLY, 
		S_IWUSR | S_IRUSR | S_IWOTH | S_IROTH);
		
	free(filename);

	// Return the file descriptor
	return fd;
}

int recvStartInitPkt(char *host, TKermitPkt* pkt, int s_timeout)
{
	int returnCode, fail_counter = 0;
	
	do {
		// Receive packet
		verboseListen(host, VERBOSE);
		returnCode = receivePkt(host, pkt, CREATE_BUF, AUTO_SIZE, s_timeout, 
								VERBOSE);

		// Error during transmission, increase fail counter
		if (returnCode < 0) 
		{
			if (pkt->data != NULL)
				free(pkt->data);
			fail_counter++;
		} 

		// Too many fails, return TIMEOUT
		if (fail_counter >= MAX_NUM_FAILS)
		{
			fprintf(stderr, "[%s] Too many fails! Abort!\n", host);
			return E_TIMEOUT;
		}

		// If packet has wrong checkkSum, then drop it and wait for next one
		if (returnCode == E_CHKSUM)
		{
			pkt->seq = nextSeq(pkt->seq);
			verboseNAK(host, pkt->seq, VERBOSE);
			sendNAK(pkt->seq);
			continue;
		}

		// Send-Init packet successfuly received. Send first ACK.
		if (pkt->type != 0) 
		{
			pkt->seq = nextSeq(pkt->seq);
			verboseACK(host, pkt->seq, VERBOSE);	
			sendFirstACK(pkt->seq);
			break;
		}
	} while (1);

	return SUCCESS;
}

int interpretPkt(int fd, TKermitPkt* pkt)
{
	int dataLen = pkt->len - 3 * sizeof(byte) - sizeof(unsigned short);

	switch (pkt->type) 
	{
		case TYPE_F:
			fd = createFile((void*)pkt->data, dataLen);
			if (fd < 0)
				return -1;
			break;

		case TYPE_D:
			if (fd < 0)
				return -1;
			write(fd, pkt->data, dataLen);
			break;

		case TYPE_Z:
			close(fd);
			fd = END_FILE;
			break;

		case TYPE_B:
			return END_TRANS;

		default:
			return E_INVALID;
	}

	return fd;
}

void sendLast(char* host, byte seq, int last)
{
	if (last == 0) 
	{
		verboseNAK(host, seq, VERBOSE);
		sendNAK(seq);
	}
	else if (last == 1) 
	{
		verboseACK(host, seq, VERBOSE);
		sendACK(seq);
	}
}

int useData(char* host, TKermitPkt* recPkt, int fd)
{
	if (recPkt->type != TYPE_S)
	{
		fd = interpretPkt(fd, recPkt);
		if(fd == END_TRANS)
			return END_TRANS;
		if (fd < 0 && fd != END_FILE) 
		{
			perror("Error");
			return E_INVALID;
		}
	}

	verboseACK(host, nextSeq(recPkt->seq), VERBOSE);
	sendACK(nextSeq(recPkt->seq));

	return fd;
}

int checkSeq(byte lastSeq, byte newSeq) 
{
	if (nextSeq(lastSeq) == 0 || nextSeq(lastSeq) == 1)
		return nextSeq(lastSeq) >= newSeq;
	return nextSeq(lastSeq) <= newSeq;
}

int listenWithCheck(char *host, byte* seq, int s_timeout, int s_bufSize)
{
	int fail_counter = 0, returnCode, fd = END_FILE, last = -1;
	TKermitPkt recPkt;
	
	do {
		// Receive packet
		verboseListen(host, VERBOSE);
		if (recPkt.data == NULL)
			returnCode = receivePkt(host, &recPkt, CREATE_BUF, s_bufSize,
									s_timeout, VERBOSE);
		else 
			returnCode = receivePkt(host, &recPkt, OLD_BUF, s_bufSize,
									s_timeout, VERBOSE);

		// Error during transmission
		if (returnCode < 0)		
			fail_counter++;

		// Check sequence number
		if ((returnCode >= 0 || returnCode == E_CHKSUM) 
				&& !checkSeq(*seq, recPkt.seq)
				&& fail_counter <= MAX_NUM_FAILS) {
			verboseString(host, "Droped packet", VERBOSE);
			continue;
		}

		// Packet successfuly received. Use its data.
		if (returnCode >= 0 && fail_counter <= MAX_NUM_FAILS) {				
			last = 1;
			fail_counter = 0;
			*seq = nextSeq(recPkt.seq);
			if ((fd = useData(host, &recPkt, fd)) == END_TRANS)
				break;
			else if (fd == E_INVALID)
				return E_INVALID;
			continue;
		}

		// If packet has wrong checkkSum, then drop it and wait for next one
		if (returnCode == E_CHKSUM && fail_counter <= MAX_NUM_FAILS) {
			last = 0;
			recPkt.seq = nextSeq(recPkt.seq);
			*seq = recPkt.seq;
			verboseNAK(host, recPkt.seq, VERBOSE);
			sendNAK(recPkt.seq);
			continue;
		}

		// For timeout, retransmit last ACK/NAK
		if (returnCode == E_TIMEOUT)
			sendLast(host, *seq, last);
			
		// Too many fails, return TIMEOUT
		if (fail_counter > MAX_NUM_FAILS) {
			fprintf(stderr, "[%s] Too many fails! Abort!\n", host);
			return E_TIMEOUT;
		}
	} while (1);

	return SUCCESS;
}

int main(int argc, char** argv) {
	int s_timeout =  TIME * _1_SEC_, s_bufSize = 0;
    TKermitPkt pkt;
    byte seq;
    
	// Init sender
    init(HOST, PORT);

	// Init connection (receive SendInit packet and send ACK)
	if (recvStartInitPkt(argv[0], &pkt, s_timeout) < 0)
		return -1;

	// Set seqence, timeout, buffer size
	seq = pkt.seq;
	s_timeout = ((TSYData*)pkt.data)->time * _1_SEC_;
	s_bufSize = ((TSYData*)pkt.data)->maxl;
	free(pkt.data);

	// Listen for data
	if (listenWithCheck(argv[0], &seq, s_timeout, s_bufSize) < 0)
		return -1;

	// Send End of transmission ACK
	verboseString(argv[0], "-------- END TRANSMISSION --------", VERBOSE);
	verboseACK(argv[0], seq, VERBOSE);
	sendACK(seq++);

	return 0;
}