#include "common.h"

static struct sockaddr_in server_addr;
socklen_t server_len = sizeof (server_addr);

/* Read commands */
int parse_function(char *buffer, char ***argu)
{
	int idx = 0, jdx = 0;
	int length = strlen(buffer);

	/* Pass the charcters of command */
	while (idx <= length)
	{
		/*check for space and null character */
		if (buffer[idx] == ' ' || buffer[idx] == '\0')
		{
			buffer[idx] = '\0';

			/* Memory allocation for addrs */
			if (jdx)
				*argu = realloc(*argu, (jdx + 1) * sizeof (char *));
			else
				*argu = malloc(sizeof (char *));

			/* Memory allocation for string and store the string */
			*(*argu + jdx) = malloc(strlen(buffer) + 1);
			strcpy(*(*argu + jdx), buffer);

			jdx++;
			idx++;

			while (!(buffer[idx] != '\0' && buffer[idx] != ' ' && buffer[idx] != '\t'))
				idx++;

			buffer = buffer + idx;
		}
		else
			idx++;
	}

	/* Make last argument NULL */
	*(*argu + jdx) = NULL;

	/*return the index*/
	return jdx;
}

/* Send request to sever */
int send_request(int sock_fd, int req, char *file)
{
	req_pack_t req_pack;
	packet_t packet;
	fd_set fdset;
	struct timeval tv;
	int retval;


	/* Store the req details to req packet */
	req_pack.opcode = req;
	strcpy(req_pack.fname, file);
	strcpy(req_pack.mode, "netascii");

	printf("Sending request to server\n");

	/* Send the req packet */
	sendto(sock_fd, (void *) &req_pack, sizeof (req_pack), 0, (struct sockaddr *)&server_addr, sizeof (server_addr));

	recvfrom(sock_fd, (void *) &packet, sizeof (packet), 0, (struct sockaddr *) &server_addr, &server_len);

	/* check for error packet */
	if (packet.rpack.opcode == ERROR)
	{
		printf("Error : %s\n", packet.epack.error_msg);
		return 0;
	}

	/* ACK packet return 1 for further proceed */
	else if (packet.rpack.opcode == ACK)
	{
		printf("Request acknowledgement received from server \n");
		return 1;
	}
}

/* Open the files */
int file_open(char *file, int flag)
{
	int fd, fd_flags;

	/* Create and Open a file in write only mode */
	if (flag == CREATE)
	{
		fd_flags = O_CREAT|O_WRONLY|O_EXCL;

		/* Error handler */
		if ((fd = open(file, fd_flags, 0777)) == -1)
		{
			if (errno == EEXIST)
			{
				printf("File %s already exist\n", file);
				return 0;
			}

			perror("open");
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
			printf("File %s does not exist\n", file);
			return 0;
		}	
	}

	/* return fd */
	return fd;
}

// Get file from server 
void get_file_from_server(int sock_fd, int fd)
{
	int rbytes;
	packet_t packet;
	ack_pack_t ack_pack;
	fd_set fdset;
	struct timeval tv;
	int retval, index = 0;

	/* Write the received content to the file */
	while (1)
	{
		FD_ZERO(&fdset);
		FD_SET(sock_fd, &fdset);
		tv.tv_sec = 1;
		tv.tv_usec = 0;

		retval = select(sock_fd + 1, &fdset, NULL, NULL, &tv);

		/* Receive the content */
		if (retval == -1)
			perror("select()");

		else if (retval)
		{
			rbytes = recvfrom(sock_fd, (void *) &packet, sizeof (packet), 0, NULL, NULL);

			/* Data packet then write the content to the file */
			if (packet.rpack.opcode == DATA)
			{
				if (packet.dpack.block_num == (ack_pack.block_num + 1))
				{
					write(fd, packet.dpack.data, rbytes - 4);
					printf("Received data packet%d successfully\n", packet.dpack.block_num);
					printf("Sending acknowledgement for %d\n", packet.dpack.block_num);
				}
				else
					printf("Resending acknowledgement for %d\n", packet.dpack.block_num);

				/* Store the details to ACK packtet*/
				ack_pack.opcode = ACK;
				ack_pack.block_num = packet.dpack.block_num;

				index++;
				/* Send the ACK packet */
				if (index != 4970)
					sendto(sock_fd, (void *) &ack_pack, sizeof (ack_pack), 0, (struct sockaddr *)&server_addr, sizeof (server_addr));
			}

			/* STOP of content and break the loop */
			else if (packet.rpack.opcode == STOP)
				break;
		}
	}
}

/* Put the file to server */
void put_file_to_server(int sock_fd, int fd)
{
	int count = 0, bytes = 0;
	packet_t packet;
	data_pack_t data_pack;
	fd_set fdset;
	struct timeval tv;
	int retval, flag = 0;

	FD_ZERO(&fdset);
	FD_SET(sock_fd, &fdset);
	tv.tv_sec = 1;
	tv.tv_usec = 0;

	data_pack.opcode = DATA;

	/* Read the content and send to server */
	while (1)
	{
		/*read the data */
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
			/* Send the Data packet */
			data_pack.opcode = STOP;
			sendto(sock_fd, (void *) &data_pack, sizeof (data_pack), 0, (struct sockaddr *)&server_addr, sizeof (server_addr));
			break;
		}

		while (1)
		{
			/*send the packet*/
			sendto(sock_fd, (void *) &data_pack, bytes + 4, 0, (struct sockaddr *)&server_addr, sizeof (server_addr));

			FD_ZERO(&fdset);
			FD_SET(sock_fd, &fdset);
			tv.tv_sec = 1;
			tv.tv_usec = 0;

			/* Receive the content */
			retval = select(sock_fd + 1, &fdset, NULL, NULL, &tv);
			
			/*error handling*/
			if (retval == -1)
				perror("select()");

			else if (retval)
			{
				/*return the packet*/
				recvfrom(sock_fd, (void *) &packet, sizeof (packet), 0, NULL, NULL);
				
				/*check for block number*/
				if (packet.apack.block_num == data_pack.block_num)
				{
					/*send acknowledgement packet*/
					printf("Acknowledgement packet for %d\n", packet.apack.block_num);
					break;
				}
			}

			printf("Resending the packet %d\n", data_pack.block_num);
		}
	}
}

/* TFTP Client */
int main()
{
	char buffer[100], **argu;
	int argc, sock_fd, fd, flag = 0;

	/* Create sockfd for connectionless protocol */
	sock_fd = socket(AF_INET, SOCK_DGRAM, 0);

	/* tftp prompt */
	while (1)
	{
		memset(&buffer, 0, sizeof (buffer));

		/* Read a command */
		printf("[ tftp ] > " );
		scanf("%[^\n]", buffer);
		__fpurge(stdin);

		/* No command redisplay the command */
		if (strlen(buffer) == 0)
			continue;

		/* Read the command as commandline arguments */
		argc = parse_function(buffer, &argu);

		/* Check the validity of arguments */
		if (2 < argc)
		{
			printf("Invalid arguments: Use [help]\n");
			continue;
		}

		server_addr.sin_port = htons(SERVER_PORT);

		/* Connect to the server */
		if (strcmp(argu[0], "connect") == 0)
		{
			/* Store the server details */
			server_addr.sin_family = AF_INET;
			server_addr.sin_addr.s_addr = inet_addr(argu[1]);
			server_addr.sin_port = htons(SERVER_PORT);

			/* Set the flag if server details are updated */
			flag = 1;
		}
		/* Server details are updated then proceed further */
		if (flag)
		{
			/* Get the file from server */
			if (strcmp(argu[0], "get") == 0)
			{
				/*function call to open the file*/
				if ((fd = file_open(argu[1], CREATE)))
				{	
					/*function call to sent request*/
					if (send_request(sock_fd, RRQ, argu[1]))
					{
						/*function call to get file from server*/
						get_file_from_server(sock_fd, fd);
						close (fd);
					}
				}
			}

			/* Put the file to server */
			if (strcmp(argu[0], "put") == 0)
			{
				/*function call to open the file*/
				if((fd = file_open(argu[1], READ)))
				{
					/*function call to sent request*/
					if (send_request(sock_fd, WRQ, argu[1]))
					{
						/*function call to put file to server*/
						put_file_to_server(sock_fd, fd);
						close (fd);
					}
				}
			}
		}

		/* Error print */
		else
			printf("Connect to the server\n");

		/* Exit from prompt */
		if (strcmp(buffer, "bye") == 0 || strcmp(buffer, "quit") == 0 )
			break;
	}
	
	/*close the sock fd*/
	close (sock_fd);
	return 0;
}
