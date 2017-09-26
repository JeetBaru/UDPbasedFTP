****************************Reliable UDP File Transfer******************************

UDP(Use Datagram Protocol) is a connectionless transmission model with a minimum of
protocol mechanism. It has no handshaking dialogues and no mechanism of
acknowledgements(ACKs). Therefore, every packet delivered by using UDP protocol is not
guaranteed to be received. That’s why UDP is an unreliability protocol.

In this project, I have implemented a reliable file transfer application
based on UDP. I have implemented stop and wait algorithm to synchronise the server
and the client and ensure reliable file transfer.

The application is able to send, recieve, list and delete files on the the server.

The project contains 2 Directories: Server and Client
The Server contains server.c and a Makefile, and the Client folder contains
client.c and Makefile

To make the Server or Client we must be in the respective folder and make
makes an executable for the same.

*/Client$ make
*/Server$ make

On building and generating an executable to run the program we must:

1. The server must take one command line argument: a port number for the server to use.

	*/Server$ ./server 5000 #Running the server with port number 5001

2. The client must take two command line arguments: an IP address of the machine on which
the server application is running, and the port the server application is using.

	*/Client$ ./client 127.0.0.1 5000 #Running the client with given server IP and port number

The server here could be as the local host or could be remote without affecting the reliability

Features of the application:

The application in the begining of execution displays a menu with information regarding
the syntax to use different functionality. The menu appears like this:

	1. get <filename>
	2. put <filename>
	3. delete <filename>
	4. list
	5. exit

Get Functionality

The user must type get keyword followed by a filename to copy that file from the server to
the client directory. This command is case sensitive and anything but small case 'get' shall
result in invalid command error. The application also returns suitable error if the file at 
the server cannot be copied if it doesnot exist. Also the user must pass only one filename 
at a time as multiple filenames will result in the server copying only the first file only 
and the second file is ignored.

USAGE: get <filename>

Put Functionality

The user must type put keyword followed by a filename to copy that file from the client to
the server directory. This command is case sensitive and anything but small case 'put' shall
result in invalid command error. The application also returns suitable error if the file at 
the server cannot be copied if it doesnot exist. Also the user must pass only one filename 
at a time as multiple filenames will result in the server copying only the first file only 
and the second file is ignored.

USAGE: put <filename>

Delete Functionality

The user must type delete keyword followed by a filename to delete the file from the server and
 display suitable message. This command is case sensitive and anything but small case 'get' shall
result in invalid command error. The application also returns suitable error if the file at 
the server cannot be copied if it doesnot exist. Also the user must pass only one filename 
at a time as multiple filenames will result in the server copying only the first file only 
and the second file is ignored.

USAGE: delete <filename>

List Functionality

This function lists the detailsof all files at the server. The user must enter list only any
 word after a space after list shall be ignored. 

USAGE: list

Exit functionality

This function exits the application from both server and the client. The user must enter exit only,
any work beyond exit shall be ignored and the application shall exit at both server and client.

USAGE: exit

Invalid command

Any input from the user at the client end that does not match to the expected inputs shall result
in a invalid input error and echo the same invalid command at both server and client. It must be 
noted that any of the valid command if entered without without spaces shall result in this error.

Other

The application is also designed to handle conrner cases of 0 file size, wrong file name, wrong
command. It also ensures data integrity by incorporating a checksum. The stop and wait algorithm
shall ensure reliable transfer of data overcoming the biggest disadvantage of UDP.

