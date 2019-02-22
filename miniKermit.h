#ifndef MINI_KERMIT
#define MINI_KERMIT

/* Minimalistic error codes */
#define SUCCESS		 0
#define E_TIMEOUT	-1
#define E_CHKSUM	-2
#define E_NULL		-3
#define E_INVALID	-4
#define E_CONN		-5

/* Constants */
#define CREATE_BUF	 1
#define OLD_BUF		 0
#define AUTO_SIZE	-1
#define MAX_SEQ		64
#define NO_SEQ		-1

/* Types of packet */
#define TYPE_S	'S'			// Send-Init (S) Packet
#define TYPE_F	'F'			// File Header (F) Packet
#define TYPE_D	'D'			// Data (D) Packet
#define TYPE_Z	'Z'			// End of file (EOF) packet
#define TYPE_B	'B'			// End of trazaction (EOT) packet
#define TYPE_Y	'Y'			// "Y" packet (ACK)
#define TYPE_N	'N'			// "N" packet (NAK)
#define TYPE_E	'E'			// Error packet

/* Byte type defined as an unsigned char */
typedef unsigned char byte;

/* The structure of a MINI-KERMIT packet */
typedef struct __attribute__((packed)) {
	byte soh; 				// Start-of-Header = 0x1
	byte len;				// Length of the packet
	byte seq;				// Sequence number modulo 64
	byte type;				// Type of the packet
	byte* data;				// Data to be sent
	unsigned short check;	// Check-sum of the packet
	byte mark;				// End of Block Marker
} TKermitPkt;

/* Send-Init (S type) data or "Y" (ACK) data structure */
typedef struct __attribute__((packed)) {
	byte maxl;				// Maximum size of of data field from TKermitPkt
	byte time;				// Timeout for a packet in seconds
	byte npad; 				// Padding before each packet. Initial value is 0x0
	byte padc;				// The character used for padding. Default is 0x0
	byte eol;				// The character used in mark field from TKermitPkt
	byte qctl; 				// Not used
	byte qbin; 				// Not used
	byte chkt; 				// Not used
	byte rept; 				// Not used
	byte capa; 				// Not used
	byte r;					// Not used
} TSYData;

/* Functions available for KERMIT protocol */
TKermitPkt createKermitPkt(byte seq, const char type, byte* data, byte dataLen);
TSYData createSYData(byte maxl, byte time);
void packData(msg* msgAddr, void* data, int length);
void loadPayload(msg* message, TKermitPkt* pkt);
int extractPayload(msg message, TKermitPkt* pkt, int newBuffer, int bufMaxSize);
int receivePkt(char* host, TKermitPkt* recvPkt, int newBuffer, int bufMaxSize, 
			   int timeout, int verbose);
byte nextSeq(byte seq);

/* Verbose functions */
void verbosePkt(char* host, TKermitPkt* recvPkt);
void verboseString(char* host, char* str, int active);
void verboseSend(char* host, int seq, int active);
void verboseListen(char* host, int active);
void verboseACK(char* host, int seq, int active);
void verboseNAK(char* host, int seq, int active);

#endif