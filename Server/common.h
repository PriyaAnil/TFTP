#ifndef FTP_H
#define FTP_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/ip.h> 

#define RRQ 0
#define WRQ 1
#define DATA 2
#define ACK 3
#define ERROR 4
#define CREATE 5
#define READ 6
#define STOP 7

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 5000

typedef struct req_pack
{
	short int opcode;
	char fname[20];
	char mode[20];

} req_pack_t;

typedef struct data_pack
{
	short int opcode;
	short int block_num;
	char data[512];

} data_pack_t;

typedef struct ack_pack
{
	short int opcode;
	short int block_num;

} ack_pack_t;

typedef struct err_pack
{
	short int opcode;
	short int err_num;
	char error_msg[50];

} err_pack_t;

typedef union packet
{
	req_pack_t rpack;
	data_pack_t dpack;
	ack_pack_t apack;
	err_pack_t epack;

} packet_t;

#endif
