#include "common.h"

err_pack_t err_pack;

/*  Open the file */
int file_open(char *file, int flag)
{
	int fd, fd_flags;

	err_pack.opcode = ERROR;

	/* Create and Open a file in write only mode */
	if (flag == CREATE)
	{
		fd_flags = O_CREAT|O_WRONLY|O_EXCL;

		/* Error handler */
		if ((fd = open(file, fd_flags, 0644)) == -1)
		{
			/*Error checking*/
			if (errno == EEXIST)
			{
				printf("File already exist\n");
				strcpy(err_pack.error_msg, "File already exist in server");
				return 0;
			}

			return 0;
		}
	}

	/* Open a file in read only mode */
	else if (flag == READ)
	{
		fd_flags = O_RDONLY;

		/* Error handler */
		if ((fd = open(file, fd_flags)) == -1)
		{
			printf("File is not exist\n");
			
			/*Send the error message*/
			err_pack.opcode = ERROR;
			strcpy(err_pack.error_msg, "File not exist in server");
			return 0;
		}	
	}

	/* return fd */
	return fd;
}

/* Get file from client */
void recv_file_from_client(int sock_fd, struct sockaddr_in c_addr, int fd)
{
	int bytes;
	packet_t packet;
	ack_pack_t ack_pack;
	fd_set fdset;
	struct timeval tv;
	int retval, idx = 0;

	while (1)
	{
		FD_ZERO(&fdset);
		FD_SET(sock_fd, &fdset);
		tv.tv_sec = 1;
		tv.tv_usec = 0;

		retval = select(sock_fd + 1, &fdset, NULL, NULL, &tv);

		/*Error checking*/
		if (retval == -1)
			perror("select()");

		else if (retval)
		{
		/* Receive the content */
			bytes = recvfrom(sock_fd, (void *) &packet, sizeof (packet), 0, NULL, NULL);

			/* check the received packet is data packet */
			if (packet.rpack.opcode == DATA)
			{
				if (packet.dpack.block_num == (ack_pack.block_num + 1))
				{
					/*if it is data packet then write the content*/
					write(fd, packet.dpack.data, bytes - 4);	
					printf("Received data packet %d successfully\n", packet.dpack.block_num);
					printf("Sending acknowledgement for packet %d\n", packet.dpack.block_num);
				}
				else
					printf("Resending acknowledgement for packet %d\n", packet.dpack.block_num);

				/* Store the details to ACK packtet*/
				ack_pack.opcode = ACK;
				ack_pack.block_num = packet.dpack.block_num;

				idx++;

				/*To block the acknowledgement packet*/
				if (idx != 2000)
					sendto(sock_fd, (void *) &ack_pack, sizeof (ack_pack), 0, (struct sockaddr *)&c_addr, sizeof (c_addr));
			}
			
			/*Send the stop packet*/
			else if (packet.rpack.opcode == STOP)
				break;
		}
	}
}

/* send the file to client */
void send_file_to_client(int sock_fd, struct sockaddr_in c_addr, int fd)
{
	int count = 0, bytes = 0;
	packet_t packet;
	data_pack_t data_pack;
	fd_set fdset;
	struct timeval tv;
	int retval, flag = 1;

	data_pack.opcode = DATA;
	err_pack.opcode = ERROR;

	while (1)
	{
		/*Read the bytes from */
		if ((bytes = read(fd, data_pack.data, 512)) != 0)
		{
			/*error handling*/
			if (bytes == -1)
			{
				perror("read");
				return;			
			}

			count++;
			data_pack.block_num = count;

			/* Send the Data packet */
			printf("Sending the packet %d\n", count);
		}

		else
		{
			/*send stop packet*/
			data_pack.opcode = STOP;
			sendto(sock_fd, (void *) &data_pack, sizeof (data_pack), 0, (struct sockaddr *)&c_addr, sizeof (c_addr));
			break;
		}

		while (1)
		{
			/*send packet*/
			sendto(sock_fd, (void *) &data_pack, bytes + 4, 0, (struct sockaddr *)&c_addr, sizeof (c_addr));

			FD_ZERO(&fdset);
			FD_SET(sock_fd, &fdset);
			tv.tv_sec = 1;
			tv.tv_usec = 0;

			retval = select(sock_fd + 1, &fdset, NULL, NULL, &tv);
			
			/*Error handling*/
			if (retval == -1)
				perror("select()");

			else if (retval)
			{
				/*receive packet*/
				recvfrom(sock_fd, (void *) &packet, sizeof (packet), 0, NULL, NULL);
				/*check for block num and send ack*/
				if (packet.apack.block_num == data_pack.block_num)
				{
					printf("Acknowledgement received for packet %d\n", packet.apack.block_num);
					break;
				}
			}

			printf("Resending the packet %d\n", data_pack.block_num);
		}
	}
}

int main()
{
	int fd, sock_fd, data_fd, buffer_len, client_len, count = 0, new_port;
	struct sockaddr_in serv_add, client_add;
	socklen_t client_addr_size;
	packet_t packet;
	ack_pack_t ack_pack;

	printf("Server is waiting....\n");

	/*create sock fd*/
	sock_fd = socket(AF_INET, SOCK_DGRAM, 0);

	/*declare the structure*/
	serv_add.sin_family = AF_INET;
	serv_add.sin_addr.s_addr = inet_addr(SERVER_IP);
	serv_add.sin_port =  htons(SERVER_PORT);

	/*initialise the all members to null*/
	memset(serv_add.sin_zero, '\0', sizeof(serv_add.sin_zero));

	// Bind a name to a socket
	bind(sock_fd, (struct sockaddr*)&serv_add, sizeof(serv_add));

	client_addr_size = sizeof(client_add);

	while (1)
	{
		/*receive the packet*/
		recvfrom(sock_fd, (void *)&packet, sizeof (packet), 0, (struct sockaddr*)&client_add, &client_addr_size);

		count++;

		/*assign with new port*/
		new_port = SERVER_PORT + count;

		switch (fork())
		{
			case -1:
				
				/*Error handling*/
				perror("fork()");
				exit(-1);

			case 0:
				
				/*close the sock fd*/
				close (sock_fd);
				sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
				
				/*assign with the new port number*/
				serv_add.sin_port = htons(new_port);
				
				/*bind with new port number */
				bind(sock_fd, (struct sockaddr *) &serv_add, sizeof (serv_add));
				printf("%s\n", inet_ntoa(serv_add.sin_addr));
				printf("%d\n", ntohs(serv_add.sin_port));
				
				/*check for read request packet*/
				if (packet.rpack.opcode == RRQ)
				{
					/*open the file in read mode*/
					if ((fd = file_open(packet.rpack.fname, READ)))
					{
						ack_pack.opcode = ACK;
						ack_pack.block_num = 0;
						
						/*send ack packet*/
						sendto(sock_fd, (void *)&ack_pack, sizeof (ack_pack), 0, (struct sockaddr*)&client_add, sizeof(client_add));
						printf("Acknowledgement sent successfully\n");
						
						/*call function to send file to client*/
						send_file_to_client(sock_fd, client_add, fd);
						close(fd);
					}

					else
						{
						/*Else send the error packet*/
						printf("Sending error packet\n");
						sendto(sock_fd, (void *)&err_pack, sizeof (err_pack), 0, (struct sockaddr*)&client_add, sizeof(client_add));
						}
				}
				/*chcek for the write request*/
				else if (packet.rpack.opcode == WRQ)
				{
					/*function call to open the file in write mode*/
					if ((fd = file_open(packet.rpack.fname, CREATE)))
					{
						ack_pack.opcode = ACK;
						ack_pack.block_num = 0;
						
						/*send the ack packet*/
						sendto(sock_fd, (void *)&ack_pack, sizeof (ack_pack), 0, (struct sockaddr*)&client_add, sizeof(client_add));
						printf("Acknowledgement sent successfully\n");

						/*function call to receive the file from client*/
						recv_file_from_client(sock_fd, client_add, fd);

						close(fd);
					}

					else
					{
						/*else sent error packet*/
						printf("Sending error packet\n");
						sendto(sock_fd, (void *)&err_pack, sizeof (err_pack), 0, (struct sockaddr*)&client_add, sizeof(client_add));
					}
				}

				close(sock_fd);

				exit(0);

			default:
				;
		}
	}

	return 0;
}


