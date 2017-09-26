#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/time.h>
#include <stdlib.h>
#include <memory.h>
#include <string.h>

#define MAXBUFSIZE 1000

struct sockaddr_in remote;
unsigned int remote_length;
int sock;

void send_file(char * filename);
void recieve_file(char * filename);
void delete_file(char * filename);
void list_file();

/*
It takes pointer to the payload and the length for checksum as 
input arguments

This Function calculates checksum and returnsan int checksum
It uses mod 100 checksum algorithm
*/
int checksum(char * buffer, int length)
{
        int i;
        int checksum = 0;
        for(i=0;i<length;i++)
        {
                checksum = (checksum + *buffer)%100;
        }
        return checksum;
}


/*
This is structure of packet to be sent to and from the
server and client. It consists of the sequence number and 
checksum and payload
*/
struct packet{
	int seq_no;
	int checksum;
	char payload[1000];
}packet;


/*
This function encrypts and decrypts entire files
This function uses XOR for encryption. It performs character 
by character encryption and uses a character key
*/
void crypt(char * filename)
{
	char c;
	FILE * fp;
	fp = fopen(filename,"rw");
	char key = 'j';
	c = fgetc(fp);
	while(c!=EOF)
	{
		fputc(c^key,fp);
		c = fgetc(fp);
	}	
	fclose(fp);
}

/*
This function is called list command is entered. It
generates a list.txt file containing ls -al output
and this file is sent back to the client.
*/
void list_file()
{
	system("ls -al > list.txt");
	send_file("list.txt");
	remove("list.txt");
}

/*
This function is used to decode the entered command and
call respective functions. 

The user input is broken into command and filename here

If an invalid command is entered 
this function echos the invalif command
*/
void decode_command(char buffer[])
{
	char invcom[50];
	strcpy(invcom,buffer);
	
	printf("%s",buffer);
	
	char * command;
	char * filen;
	command = strtok(buffer, "\n "); 	//command extracted
	filen = strtok(NULL, "\n ");		//filename extracted
	
	if(strcmp(command,"put")==0)
	{
		recieve_file(filen);
	}
	else if(strcmp(command,"get")==0)
	{
		send_file(filen);
	}
	else if(strcmp(command,"delete")==0)
	{
		delete_file(filen);
	}
	else if(strcmp(command,"list")==0)
	{
		list_file();
	}
	else if(strcmp(command,"exit")==0)
	{
		printf("Exiting\n");
		close(sock);			//close sock before exit
		exit(0);
	}	
	else					//echo invalid command
	{
		printf("Invalid Command\n%s",invcom);
		sendto(sock, invcom, strlen(invcom), 0, (struct sockaddr *)&remote, remote_length);
	}
	
}

/*
This function deletes the file whose name is passed
as input parameter
This function checksif file exits and accordingly deletes
or returns appropriate error message
*/
void delete_file(char *filename)
{
	printf("Deleting %s\n", filename);
	int r = remove(filename);
	char message[50];
	int nbytes;
	
	bzero(message,strlen(message));

	//check if file exists and if it was deleted
	if(r >= 0)
		sprintf(message,"Deleted %s sucessfully\n",filename);
	else
		sprintf(message,"File %s cannot be deleted\n",filename);

	//respond with appropriate message
	nbytes = sendto(sock, message, strlen(message), 0, (struct sockaddr *)&remote, remote_length);

	printf("%s",message);
}

/*
This function sends the file whose filename is passed as 
input parameters

The function checks if file exists and responds accordingly

The function also sends packets according to the size of the file

The file is first encrypted before sending and decrypted after sending
*/
void send_file(char * filename)
{
	char ack [20];
	char asci[200];
	char message[50];
	bzero(message,sizeof(message));

	int flag = 0;

	struct packet p;

	int nbytes;
	long seek_position = 0;
	long buffer_size=5000;	
	long bytes_read=0;

	//check if file exists
	if(access(filename,F_OK) == -1)
	{
		sprintf(message,"File does not exist");
		sendto(sock, message, strlen(message), 0, (struct sockaddr *)&remote, remote_length);
		printf("%s\n",message);
		return;
	}
	else
	{
		sprintf(message,"File exists");
		sendto(sock, message, strlen(message), 0, (struct sockaddr *)&remote, remote_length);
	}	

	//encrypt the file
	crypt(filename);
	
	FILE * fp;

        fp=fopen(filename,"r");

	fseek(fp, 0, SEEK_END);
	long lsize = ftell(fp);
	printf("File Size = %ld\n",lsize);
	
	sprintf(asci,"%ld",lsize);

	//send file size to reciever
	nbytes = sendto(sock, asci, strlen(asci), 0, (struct sockaddr *)&remote, remote_length);	
	
	//report error if file size is zero
	if(lsize == 0)
	{
		printf("0 File Size Error\n");
		fclose(fp);
		return;
	}

	//set time out for recvfrom() so as to resend for packet/ack drop
	struct timeval tv;
        tv.tv_sec = 1;  /* 1 Secs Timeout */
        tv.tv_usec = 0;  // Not init'ing this can cause strange errors
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv,sizeof(struct timeval));

	rewind(fp);
	
	//for large files
	if(lsize > sizeof(p.payload))
	{
		p.seq_no = 1;
		p.checksum = 0;
		//for all packets that can possibly take the size of the buffer
		while(bytes_read < (lsize-sizeof(p.payload)))
		{
			printf("Sending Packet Number: %d\n",p.seq_no);

			//send until ack recieved
			while(flag == 0)
			{
				//read file
				fseek(fp,seek_position,SEEK_SET);
				fread(p.payload,sizeof(p.payload),1,fp);
				p.checksum = checksum(p.payload,sizeof(p.payload));

				nbytes = sendto(sock, (void *)&p, sizeof(p), 0, (struct sockaddr *)&remote, remote_length);
				
				bzero(ack,sizeof(ack));
				nbytes = recvfrom(sock, ack, 1000, 0, (struct sockaddr *)&remote, &remote_length);

				if(nbytes>0 && strcmp(ack,"recieved")==0)
				{	
					//if ack recieved set flag to 1 and read next chunk	
					flag = 1;
					p.seq_no++;
					bytes_read = bytes_read + sizeof(p.payload);
					seek_position = seek_position + sizeof(p.payload);
				}
			}
			flag = 0;
		}
		//last packet with size smaller than max packet size
		fseek(fp,seek_position,SEEK_SET);
        	fread(p.payload,lsize,1,fp);
		p.checksum = checksum(p.payload,(lsize-seek_position));
		printf("Sending Packet Number: %d\n",p.seq_no);
		
		//resend till ack is recieved
		while(flag == 0)
		{	
			nbytes = sendto(sock, (void *)&p, sizeof(p), 0, (struct sockaddr *)&remote, remote_length);
			bzero(ack,sizeof(ack));
			nbytes = recvfrom(sock, ack, 1000, 0, (struct sockaddr *)&remote, &remote_length);
			if(nbytes > 0 && strcmp(ack,"recieved") == 0)
				flag = 1;
		}
		flag = 0;
	}
	//for small file size
	else
	{
		p.seq_no = 1;
		p.checksum = 0;
		printf("Sending Packet Number : %d\n",p.seq_no);

		fseek(fp,seek_position,SEEK_SET);
	        fread(p.payload,lsize,1,fp);
		p.checksum = checksum(p.payload,lsize);
		//send untill ack is recieved
		while(flag == 0)
		{
			nbytes = sendto(sock, (void *)&p, lsize+8, 0, (struct sockaddr *)&remote, remote_length);
			bzero(ack,sizeof(ack));
			nbytes = recvfrom(sock, ack, 1000, 0, (struct sockaddr *)&remote, &remote_length);
			if(nbytes>0 && strcmp(ack,"recieved") == 0)
			{	
				flag = 1;
			}
		}
		flag = 0;
	}
	fclose(fp);

	crypt(filename);

	//Reset timeout for restof the program 
        tv.tv_sec = 0;  /* Reset Timeout */
        tv.tv_usec = 0;  // Not init'ing this can cause strange errors
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv,sizeof(struct timeval));

	printf("\n\n\t\t\tFILE SENT");
}

/*
This function recieves the filename passed as input arguement

This function displays if the file exists on the senders end

It also handles if the file to be recieved is of 0 bytes

It also handles packets for various file size

It decrypts the recieved file and also verifies checksum
*/
void recieve_file(char * file_name)
{
	int expected_seq_no = 1;

	char buffer[1000];
	bzero(buffer,sizeof(buffer));
	
	struct packet p;	

	int flag = 0;
	char ack[20];
	char message[50];
	int nbytes;

	bzero(message,sizeof(message));

	//verify if file exists
	nbytes = recvfrom(sock, message, 50, 0, (struct sockaddr *)&remote, &remote_length);

	if(strcmp(message,"File does not exist")==0)
	{	
		printf("%s\n",message);
		return;
	}

	long lsize;
	long bytes_written = 0;
	long buffer_size = 1000;

	//recieved filename and file size
	printf("Recieving File %s\n",file_name);

	nbytes = recvfrom(sock, buffer, 1000, 0, (struct sockaddr *)&remote, &remote_length);
	
	lsize = atoi(buffer);

	if(lsize == 0)
        {
                printf("0 File Size Error\n");
                return;
        }

	printf("Size of file %ld\n",lsize);

	bzero(buffer,sizeof(buffer));

	FILE * fp;
	fp = fopen(file_name,"w");

	//bytes written to file are less than total file size
	while(bytes_written<lsize)
	{
		printf("Recieving Packet Number %d\n",expected_seq_no);
		
		//recieve packet untill packet drop
		while(flag == 0)
		{
			nbytes = recvfrom(sock, (void *)&p, sizeof(p), 0, (struct sockaddr *)&remote, &remote_length);
			
			//if packet recieved is as expected then write to file and send ack
			if(nbytes>0 && p.seq_no >= expected_seq_no)
			{	
				flag = 1;
				expected_seq_no++;
				//if the packet recieved is thelast for a big file or first for a small file
				if(p.seq_no == (int)(lsize/sizeof(p.payload)) + 1)
				{
					if(p.checksum != checksum(p.payload,(lsize - ((int)lsize/sizeof(p.payload)) * sizeof(p.payload))))
                                        {
                                                printf("\t\t\tChecksum Does not match\n");
                                         }
					fwrite(p.payload,1,(lsize - ((int)lsize/sizeof(p.payload)) * sizeof(p.payload)),fp);
				}
				//if the packet is initial packet of a big file
				else
				{
					if(p.checksum != checksum(p.payload,sizeof(p.payload)))
                                        {
                                                printf("\t\t\tChecksum Does not match\n");
                                         }
					fwrite(p.payload,1,sizeof(p.payload),fp);
				}
				bytes_written = bytes_written + nbytes - 8;	
		
				nbytes = sendto(sock, "recieved", 8, 0, (struct sockaddr *)&remote, remote_length);
			}
			//ifpacket recieved is 1 less than expected meaning ack got dropped. hence dont rewrite packet, just respond with ack
			else if(nbytes>0 && (p.seq_no == expected_seq_no -1))
			{
				printf("\t\t\t\t\t\tDROPPED\n");
				flag = 1;
				nbytes = sendto(sock,"recieved",8,0,(struct sockaddr *)&remote,remote_length);
			}
		}
		flag = 0;
	}
	fclose(fp);

	crypt(file_name);

	printf("\n\n\t\t\tFILE RECIEVED");
} 

int main (int argc, char * argv[] )
{
	struct sockaddr_in sin;     //"Internet socket address structure"
	int nbytes;                  
	char buffer[1000];             //a buffer to store our received message
	if (argc != 2)
	{
		printf ("USAGE:  <port>\n");
		exit(1);
	}

	/******************
	  This code populates the sockaddr_in struct with
	  the information about our socket
	 ******************/

	bzero(&sin,sizeof(sin));                    //zero the struct
	sin.sin_family = AF_INET;                   //address family
	sin.sin_port = htons(atoi(argv[1]));        //htons() sets the port # to network byte order
	sin.sin_addr.s_addr = INADDR_ANY;           //supplies the IP address of the local machine

	//Causes the system to create a generic socket of type UDP (datagram)
	if ((sock = socket(AF_INET, SOCK_DGRAM, 0))< 0)
		printf("unable to create socket");
	
	/******************
	  Once we've created a socket, we must bind that socket to the 
	  local address and port we've supplied in the sockaddr_in struct
	 ******************/
	if (bind(sock, (struct sockaddr *)&sin, sizeof(sin)) < 0)
		printf("unable to bind socket\n");
	

	remote_length = sizeof(remote);

	//waits for an incoming message
	bzero(buffer,sizeof(buffer));
	nbytes = recvfrom(sock,buffer,1000,0,  (struct sockaddr *)&remote, &remote_length);

	//handshaking with hello world
	printf("Client says %s\n", buffer);
	
	char msg[] = "World";
	
	nbytes = sendto(sock, msg, strlen(msg), 0, (struct sockaddr *)&remote, remote_length);
	
	printf("Server sent World\n");
	
	// Run the code continuously untill exit command is typed
	while(1)
	{
		printf("\n\n\t\t\tWaiting for command\n\n");
	
		bzero(buffer,sizeof(buffer));

		nbytes = recvfrom(sock,buffer,1000,0,  (struct sockaddr *)&remote, &remote_length);

		decode_command(buffer);
	}	
	close(sock);
}

