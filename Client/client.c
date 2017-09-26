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
#include <errno.h>

#define MAXBUFSIZE 1000

struct sockaddr_in remote, from_addr;              //"Internet socket address structure"
int sock;
int addr_length;

void send_file(char * filename);
void recieve_file(char * filename);
void list_file();
void delete_file(char * filename);

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
struct packet
{
	int seq_no;
	int checksum;
	char payload[1000];
};

/*
This function deletes the file whose name is passed
as input parameter
This function recieves appropriate error message if 
file was deleted at the server
*/
void delete_file(char * filename)
{
	int nbytes;
	
	char message[50];
	bzero(message,sizeof(message));

	//Display appropriate message if file was deleted	
	nbytes = recvfrom(sock,message,50,0,  (struct sockaddr *)&from_addr, &addr_length);

	printf("%s",message);	
}

/*
This function encrypts and decrypts entire files
This function uses XOR for encryption. It performs character 
by character encryption and uses a character key
*/
void crypt(char * filename)
{
	char c;
	char key = 'j';
	FILE * fp;
	fp = fopen(filename,"rw");

	c = getc(fp);
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
at the server and this file is displayed to the client.
*/
void list_file()
{
	recieve_file("list.txt");

	char c;

	FILE * fp;
	fp = fopen("list.txt","r");

	//display the list.txt file	
	c = fgetc(fp);
	while(c!=EOF)
	{
		printf("%c",c);
		c = fgetc(fp);
	}

	fclose(fp);
	
	remove("list.txt");
}

/*
This function is used to decode the entered command and
call respective functions. 

The user input is broken into command and filename here

If an invalid command is entered 
this function echos the invalif command
*/
void decode_command(char com[])
{
	char * command;
	char * filename;
	char buffer[MAXBUFSIZE];
	command = strtok(com, "\n ");		//command extracted
	filename = strtok(NULL, "\n ");		//filename extracted

	if(strcmp(command,"put")==0)
	{
		send_file(filename);
	}
	else if(strcmp(command,"get")==0)
	{
		recieve_file(filename);
	}
	else if(strcmp(command,"delete")==0)
	{
		delete_file(filename);
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
		char message [50];
		bzero(message,sizeof(message));
		printf("Invalid Command\n");
		recvfrom(sock,message,50,0,  (struct sockaddr *)&from_addr, &addr_length);
		printf("%s",message);
	}
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
	int remote_length = sizeof(struct sockaddr);

        struct packet p;

        int nbytes;
        long seek_position = 0;
        long buffer_size=1000;
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
	
	printf("Sending File %s\n",filename);
	
        FILE * fp;

        fp=fopen(filename,"r");

        fseek(fp, 0, SEEK_END);
        long lsize = ftell(fp);
        printf("File size = %ld\n",lsize);
	
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
                        printf("Sending Packet Number %d\n",p.seq_no);
			//send until ack recieved
                        while(flag == 0)
                        {
                                fseek(fp,seek_position,SEEK_SET);
                                fread(p.payload,sizeof(p.payload),1,fp);
				p.checksum = checksum(p.payload,sizeof(p.payload));
	
                                nbytes = sendto(sock, (void *)&p, sizeof(p), 0, (struct sockaddr *)&remote, remote_length);

      				bzero(ack,sizeof(ack));

                                nbytes = recvfrom(sock, ack, 1000, 0, (struct sockaddr *)&from_addr, &addr_length);

                                if(nbytes>0 && strcmp(ack,"recieved")==0)
                                {
					//if ack recieved set flag to 1 and read next chunk     
                                        flag = 1;
                                        p.seq_no++;
                                        bytes_read = bytes_read + sizeof(p.payload);
                                        seek_position = seek_position + sizeof(p.payload);
                                }
                                else
                                {
//                                        printf("\t\t\t\t TIMEOUT  ");
                                }
                        }
                        flag = 0;
                }
                //last packet with size smaller than max packet size
		printf("Sending Packet Number %d\n",p.seq_no);
                fseek(fp,seek_position,SEEK_SET);
                fread(p.payload,lsize,1,fp);
		p.checksum = checksum(p.payload,(lsize-seek_position));
                //resend till ack is recieved
                while(flag == 0)
                {
                        nbytes = sendto(sock, (void *)&p, sizeof(p), 0, (struct sockaddr *)&remote, remote_length);
			bzero(ack,sizeof(ack));
                        nbytes = recvfrom(sock, ack, 1000, 0, (struct sockaddr *)&from_addr, &addr_length);
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
		printf("Sending Packet Number %d\n",p.seq_no);

                fseek(fp,seek_position,SEEK_SET);
                fread(p.payload,lsize,1,fp);
		p.checksum = checksum(p.payload,lsize);
                
                //send untill ack is recieved
		while(flag == 0)
                {
                        nbytes = sendto(sock, (void *)&p, lsize+8, 0, (struct sockaddr *)&remote, remote_length);
			bzero(ack,sizeof(ack));
                        nbytes = recvfrom(sock, ack, 1000, 0, (struct sockaddr *)&from_addr, &addr_length);
                        if(nbytes>0 && strcmp(ack,"recieved") == 0)
                        {
                                flag = 1;
                        }
                }
                flag = 0;
        }
        fclose(fp);

        //Reset timeout for restof the program
        tv.tv_sec = 0;  /* 30 Secs Timeout */
        tv.tv_usec = 0;  // Not init'ing this can cause strange errors
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv,sizeof(struct timeval));

	crypt(filename);	

	printf("\n\n\t\t\t FILE SENT");
}


/*
This function recieves the filename passed as input arguement

This function displays if the file exists on the senders end

It also handles if the file to be recieved is of 0 bytes

It also handles packets for various file size

It decrypts the recieved file and also verifies checksum
*/
void recieve_file(char * filename)
{
	int expected_seq_no=1;
	int flag = 0;
	char ack[20];

	struct packet p;

	char message[50];
	bzero(message,sizeof(message));

	struct sockaddr_in from_addr;
	int addr_length = sizeof(struct sockaddr);

	char buffer[1000];

	bzero(buffer,sizeof(buffer));
	
	int nbytes;
	long lsize;
	long bytes_written = 0;
	long buffer_size = 1000;

	nbytes = recvfrom(sock, message, 50, 0, (struct sockaddr *)&from_addr, &addr_length);
        //verify if file exists
        if(strcmp(message,"File does not exist")==0)
        {
                printf("%s\n",message);
                return;
        }

        printf("%s\n",message);	

        //recieved filename and file size
	printf("Recieving File %s\n",filename);

	nbytes = recvfrom(sock,buffer,1000,0,  (struct sockaddr *)&from_addr, &addr_length);

	lsize = atoi(buffer);

	printf("File Size %ld\n",lsize);
	
	if(lsize == 0)
	{
		printf("0 File Size Error\n");
		return;
	}
		
	bzero(buffer,sizeof(buffer));

	FILE * fp;
	fp = fopen(filename,"w"); 
        //bytes written to file are less than total file size
	while(bytes_written < lsize)
	{

                //recieve packet untill packet drop
		while(flag == 0)
		{
			nbytes = recvfrom(sock,(void *)&p,sizeof(p),0,  (struct sockaddr *)&from_addr, &addr_length);
		
			printf("Recieving Packet Number %d\n",p.seq_no);

                        //if packet recieved is as expected then write to file and send ack
			if(nbytes>0 && p.seq_no == expected_seq_no)
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
		
				nbytes = sendto(sock, "recieved", 8 , 0, (struct sockaddr *)&remote, sizeof(struct sockaddr));
			}
			//ifpacket recieved is 1 less than expected meaning ack got dropped. hence dont rewrite packet, just respond with ack
			else if(nbytes>0 && (p.seq_no == expected_seq_no - 1))
			{
				flag = 1;
				nbytes = sendto(sock,"recieved", 8,0, (struct sockaddr *)&remote,sizeof(struct sockaddr));
			}
		}
		flag = 0;
	}	
	fclose(fp);

	crypt(filename);

	printf("\n\n\t\t\t FILE RECIEVED");
}

int main (int argc, char * argv[])
{

	int nbytes;                             // number of bytes send by sendto()
	char buffer[MAXBUFSIZE];

	if (argc < 3)
	{
		printf("USAGE:  <server_ip> <server_port>\n");
		exit(1);
	}

	/******************
	  Here we populate a sockaddr_in struct with
	  information regarding where we'd like to send our packet 
	  i.e the Server.
	 ******************/
	bzero(&remote,sizeof(remote));               //zero the struct
	remote.sin_family = AF_INET;                 //address family
	remote.sin_port = htons(atoi(argv[2]));      //sets port to network byte order
	remote.sin_addr.s_addr = inet_addr(argv[1]); //sets remote IP address

	//Causes the system to create a generic socket of type UDP (datagram)
	if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
	{
		printf("unable to create socket");
	}

	/******************
	  sendto() sends immediately.  
	  it will report an error if the message fails to leave the computer
	  however, with UDP, there is no error if the message is lost in the network once it leaves the computer.
	 ******************/


	char command[] = "Hello";
	printf("Client sent Hello\n");	

	nbytes = sendto(sock, command, strlen(command), 0, (struct sockaddr *)&remote, sizeof(struct sockaddr));

	addr_length = sizeof(struct sockaddr);
	bzero(buffer,sizeof(buffer));

	nbytes = recvfrom(sock,buffer,1000,0,  (struct sockaddr *)&from_addr, &addr_length);

	printf("Server says %s\n", buffer);

	//Run the code untill exit command is typed
	while(1)
	{	
		
		printf("\n\n\n\t\t\t*****UDP Based FTP*****\n");
		printf("\n\t\t\t1. get <filename>");
		printf("\n\t\t\t2. put <filename>");
		printf("\n\t\t\t3. delete <filename>");
		printf("\n\t\t\t4. list");
		printf("\n\t\t\t5. exit");
		printf("\n\n\t\t\tEnter Command\n\n\n");
		fgets(command,100,stdin);	

		nbytes = sendto(sock, command, strlen(command), 0, (struct sockaddr *)&remote, sizeof(struct sockaddr));
	
		decode_command(command);
	}
	close(sock);
}

