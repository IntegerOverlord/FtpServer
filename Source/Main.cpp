#include "../JuceLibraryCode/JuceHeader.h"

#undef UNICODE

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <io.h>
#include <stdio.h>
#include <string>
#include <iostream>
#include <fcntl.h>
#include <direct.h>
#include <thread>

// Need to link with Ws2_32.lib
#pragma comment (lib, "Ws2_32.lib")
// #pragma comment (lib, "Mswsock.lib")

#define DEFAULT_BUFLEN 4096
#define DEFAULT_PORT "21"

// Gets the fd for the socket. Returns -1 on failure
// toBind - whether you bind or connect
// address - address to connect to (not used for bind)
SOCKET getSocketFD(char* s, bool toBind, std::string address)
{
	int status;
	struct addrinfo hints;
	struct addrinfo* servinfo;
	struct addrinfo* bound;
	SOCKET sock = INVALID_SOCKET;
	int yes = 1;

	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;

	//Get address info
	if (toBind)
	{
		if ((status = getaddrinfo(NULL, s, &hints, &servinfo)) != 0)
		{
			std::cout << "Error: getaddrinfo failed." << std::endl;
			return INVALID_SOCKET;
		}
	}
	else
	{
		if ((status = getaddrinfo(address.c_str(), s, &hints, &servinfo)) != 0)
		{
			std::cout << "Error: getaddrinfo failed." << std::endl;
			return INVALID_SOCKET;
		}
	}

	//Try to bind/connect

	for (bound = servinfo; bound != NULL; bound = bound->ai_next)
	{
		sock = socket(bound->ai_family, bound->ai_socktype, bound->ai_protocol);
		if (sock == INVALID_SOCKET) continue;

		if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char*)& yes, sizeof(int)) == -1)
		{
			std::cout << "Error: setsockopt failed." << std::endl;
			return INVALID_SOCKET;
		}
		if (toBind)
		{
			// try to bind
			if (bind(sock, bound->ai_addr, bound->ai_addrlen) == -1)
			{
				continue;
			}
		}
		else
		{
			//try to connect
			if (connect(sock, bound->ai_addr, bound->ai_addrlen) == -1)
			{
				continue;
			}
		}

		// If you get this far, you've successfully bound
		break;
	}

	//If you get here with a NULL bound, then you were unsuccessful
	if (bound == NULL)
	{
		std::cout << "Error: failed to bind to anything." << std::endl;
		return INVALID_SOCKET;
	}

	//Free up the space since the addrinfo is no longer needed
	freeaddrinfo(servinfo);

	//Try to listen if you're binding
	if (toBind)
	{
		if (listen(sock, SOMAXCONN) == -1)
		{
			std::cout << "Error: failed to listen." << std::endl;
			return INVALID_SOCKET;
		}
	}

	// return the fd
	return sock;
}

SOCKET getSocketFD(int p, bool toBind, std::string address)
{
	char port[6];
	sprintf(port, "%d", p);
	port[5] = '\0';
	return getSocketFD(port, toBind, address);
}

// Used to trim spaces and line feeds
std::string Trim(std::string in)
{
	bool done = false;
	for (int i = in.size() - 1; i >= 0 && !done; i--)
	{
		if (in.substr(i) == " " || in.substr(i) == "\r\n" || in.substr(i) == "\n" || in.substr(i) == "\r")
		{
			in = in.substr(0, i);
		}
		else
		{
			done = true;
		}
	}
	return in;
}

void working_thread(SOCKET command_socket)
{
	char recvbuf[DEFAULT_BUFLEN];
	int recvbuflen = DEFAULT_BUFLEN;
	int iResult;

	std::string to_send;
	bool image_mode = false;
	SOCKET data_socket = INVALID_SOCKET;
	File currentWorkingDirectory = File::getCurrentWorkingDirectory();
	std::string rename_from_filename;

	to_send = "220 Service ready for new user.\n";
	if (send(command_socket, to_send.c_str(), to_send.length(), 0) == -1)
	{
		std::cout << "Error sending." << std::endl;
	}

	// Receive until the peer shuts down the connection
	do {

		iResult = recv(command_socket, recvbuf, recvbuflen, 0);
		if (iResult > 0) {
			std::string msg = "";
			for (int x = 0; x < iResult; x++)
			{
				msg.push_back(recvbuf[x]);
			}

			std::cout << "Received: " << msg << std::endl;

			if (msg.substr(0, 4) == "USER")
			{
				// Accept all users because I'm secure like that
				to_send = "230 User logged in, proceed.\n";
				std::cout << "Answered: " << to_send << std::endl;
				send(command_socket, to_send.c_str(), to_send.size(), 0);
			}
			else if (msg.substr(0, 4) == "PASS")
			{
				// Accept all users because I'm secure like that
				to_send = "230 User logged in, proceed.\n";
				std::cout << "Answered: " << to_send << std::endl;
				send(command_socket, to_send.c_str(), to_send.size(), 0);
			}
			else if (msg.substr(0, 4) == "QUIT")
			{
				// Quit
				to_send = "221 Service closing control connection.\n";
				std::cout << "Answered: " << to_send << std::endl;
				send(command_socket, to_send.c_str(), to_send.size(), 0);
			}
			else if (msg.substr(0, 3) == "PWD")
			{
				to_send = "257 \"" + currentWorkingDirectory.getFullPathName().toStdString() + "\\\" is your current location\n";
				std::cout << "Answered: " << to_send << std::endl;
				send(command_socket, to_send.c_str(), to_send.size(), 0);
			}
			else if (msg.substr(0, 4) == "TYPE")
			{
				if (msg.substr(5, 1) == "I")
				{
					image_mode = true;
				}
				else
				{
					image_mode = false;
				}

				to_send = "200 Command okay.\n";
				std::cout << "Answered: " << to_send << std::endl;
				send(command_socket, to_send.c_str(), to_send.size(), 0);
			}
			else if (msg.substr(0, 4) == "PORT")
			{
				// open data port
				int c2 = msg.rfind(",");
				int c1 = msg.rfind(",", c2 - 1);

				// Calculate port number
				int portNumber = atoi(msg.substr(c1 + 1, c2 - c1).c_str()) * 256;
				portNumber += atoi(msg.substr(c2 + 1).c_str());

				// Get address
				std::string address = msg.substr(5, c1 - 5);
				for (int i = 0; i < address.size(); i++)
				{
					if (address.substr(i, 1) == ",")
					{
						address.replace(i, 1, ".");
					}
				}

				// Get the file descriptor
				std::cout << "Attempting to open port: " << portNumber << std::endl;
				std::cout << "From address: " << address << std::endl;
				data_socket = getSocketFD(portNumber, false, address);

				if (data_socket == INVALID_SOCKET)
				{
					// Port open failure
					to_send = "425 Can't open data connection.\n";
					std::cout << "Answered: " << to_send << std::endl;
					send(command_socket, to_send.c_str(), to_send.size(), 0);
					continue;
				}

				// Port open success
				to_send = "200 Command okay.\n";
				std::cout << "Answered: " << to_send << std::endl;
				send(command_socket, to_send.c_str(), to_send.size(), 0);
			}
			else if (msg.substr(0, 4) == "LIST")
			{
				// Now, make sure PORT has been called
				if (data_socket == INVALID_SOCKET)
				{
					// Port hasn't been set
					to_send = "451 Requested action aborted: local error in processing\n";
					std::cout << "Answered: " << to_send << std::endl;
					send(command_socket, to_send.c_str(), to_send.size(), 0);
					continue;
				}

				// Open directory
				File& directory = currentWorkingDirectory;
				std::string files = "";
				if (Trim(msg) == "LIST")
				{
					std::cout << "LIST no param" << std::endl;
				}
				else
				{
					std::cout << "LIST with param" << std::endl;
					directory = directory.getChildFile(Trim(msg.substr(5)));
				}

				DirectoryIterator iter(directory, false, "*", File::findFilesAndDirectories);
				while (iter.next())
				{
					File theFileItFound(iter.getFile());
					if (theFileItFound.isDirectory())
					{
						files += "drwxr-xr-x 1 owner group\015\012";
					}
					else
					{
						files += "-rw-r--r-- 1 owner group\015\012";
					}
					Time lastModificationTime = theFileItFound.getLastModificationTime();
					files += std::to_string(theFileItFound.getSize()) + " " + lastModificationTime.getMonthName(true).toStdString() + " "
						+ std::to_string(lastModificationTime.getDayOfMonth()) + " " + std::to_string(lastModificationTime.getHours()) + ":" + std::to_string(lastModificationTime.getMinutes())
						+ " " + theFileItFound.getFileName().toStdString() + "\n";
				}

				//tell the client that data transfer is about to happen
				to_send = "125 Data connection already open; transfer starting.\n";
				std::cout << "Answered: " << to_send << std::endl;
				send(command_socket, to_send.c_str(), to_send.size(), 0);

				// send
				send(data_socket, files.c_str(), files.length(), 0);
				std::cout << "Answered: " << files << std::endl;
				closesocket(data_socket);
				data_socket = INVALID_SOCKET;

				// done
				to_send = "250 Requested file action okay, completed.\n";

				std::cout << "Answered: " << to_send << std::endl;
				send(command_socket, to_send.c_str(), to_send.size(), 0);
			}
			else if (msg.substr(0, 4) == "CDUP")
			{
				currentWorkingDirectory = currentWorkingDirectory.getParentDirectory();
				currentWorkingDirectory.setAsCurrentWorkingDirectory();

				to_send = "250 Okay.\n";
				std::cout << "Answered: " << to_send << std::endl;
				send(command_socket, to_send.c_str(), to_send.size(), 0);
			}
			else if (msg.substr(0, 3) == "CWD")
			{
				currentWorkingDirectory = currentWorkingDirectory.getChildFile(Trim(msg.substr(4)));
				currentWorkingDirectory.setAsCurrentWorkingDirectory();

				to_send = "250 Okay.\n";
				std::cout << "Answered: " << to_send << std::endl;
				send(command_socket, to_send.c_str(), to_send.size(), 0);
			}
			else if (msg.substr(0, 4) == "FEAT")
			{
				to_send = "211-Features:\nUTF8\n211 END\n";
				std::cout << "Answered: " << to_send << std::endl;
				send(command_socket, to_send.c_str(), to_send.size(), 0);
			}
			else if (msg.substr(0, 3) == "MKD")
			{
				std::string dir_name = msg.substr(4);
				dir_name = Trim(dir_name);

				File new_dir = currentWorkingDirectory.getChildFile(dir_name);

				if (new_dir.createDirectory().wasOk())
				{
					to_send = "257 Created directory.\n";
					std::cout << "Answered: " << to_send << std::endl;
					send(command_socket, to_send.c_str(), to_send.size(), 0);
				}
				else
				{
					to_send = "451 Requested action aborted: local error in processing\n";
					std::cout << "Answered: " << to_send << std::endl;
					send(command_socket, to_send.c_str(), to_send.size(), 0);
				}
			}
			else if (msg.substr(0, 4) == "DELE")
			{
				std::string file_name = msg.substr(5);
				file_name = Trim(file_name);

				File file_to_delete = currentWorkingDirectory.getChildFile(file_name);

				if (file_to_delete.deleteFile())
				{
					to_send = "257 Deleted.\n";
					std::cout << "Answered: " << to_send << std::endl;
					send(command_socket, to_send.c_str(), to_send.size(), 0);
				}
				else
				{
					to_send = "451 Requested action aborted: local error in processing\n";
					std::cout << "Answered: " << to_send << std::endl;
					send(command_socket, to_send.c_str(), to_send.size(), 0);
				}
			}
			else if (msg.substr(0, 3) == "RMD")
			{
				std::string dir_name = msg.substr(4);
				dir_name = Trim(dir_name);

				File dir_to_delete = currentWorkingDirectory.getChildFile(dir_name);

				if (dir_to_delete.deleteRecursively())
				{
					to_send = "257 Deleted.\n";
					std::cout << "Answered: " << to_send << std::endl;
					send(command_socket, to_send.c_str(), to_send.size(), 0);
				}
				else
				{
					to_send = "451 Requested action aborted: local error in processing\n";
					std::cout << "Answered: " << to_send << std::endl;
					send(command_socket, to_send.c_str(), to_send.size(), 0);
				}

				to_send = "257 Deleted.\n";
				std::cout << "Answered: " << to_send << std::endl;
				send(command_socket, to_send.c_str(), to_send.size(), 0);
			}
			else if (msg.substr(0, 4) == "RNFR")
			{
				std::string name = msg.substr(5);
				rename_from_filename = Trim(name);

				to_send = "250 Okay\n";
				std::cout << "Answered: " << to_send << std::endl;
				send(command_socket, to_send.c_str(), to_send.size(), 0);
			}
			else if (msg.substr(0, 4) == "RNTO")
			{
				std::string name = msg.substr(5);
				name = Trim(name);

				File file_to_rename = currentWorkingDirectory.getChildFile(rename_from_filename);
				File new_file = currentWorkingDirectory.getChildFile(name);

				if (file_to_rename.moveFileTo(new_file))
				{
					to_send = "250 Okay\n";
					std::cout << "Answered: " << to_send << std::endl;
					send(command_socket, to_send.c_str(), to_send.size(), 0);
				}
				else
				{
					to_send = "451 Requested action aborted: local error in processing\n";
					std::cout << "Answered: " << to_send << std::endl;
					send(command_socket, to_send.c_str(), to_send.size(), 0);
				}
			}
			else if (msg.substr(0, 4) == "STOR")
			{
				// Now, make sure PORT has been called
				if (data_socket == INVALID_SOCKET)
				{
					// Port hasn't been set
					std::cout << "Attempted to STOR before porting." << std::endl;
					to_send = "451 Requested action aborted: local error in processing\n";
					std::cout << "Answered: " << to_send << std::endl;
					send(command_socket, to_send.c_str(), to_send.size(), 0);
					continue;
				}

				// get file
				std::string fileName = msg.substr(5);
				// Take out those spaces
				fileName = Trim(fileName);
				File file_to_create = currentWorkingDirectory.getChildFile(fileName);

				if (file_to_create.create().wasOk())
				{
					FileOutputStream output_stream(file_to_create);
					if (output_stream.openedOk())
					{
						output_stream.setPosition(0);
						output_stream.truncate();

						// Tell the client that reading is about to start
						to_send = "125 Data connection already open; transfer starting.\n";
						send(command_socket, to_send.c_str(), to_send.size(), 0);

						int bytes_read = 0;

						while ((bytes_read = recv(data_socket, recvbuf, recvbuflen, 0)) > 0)
						{
							output_stream.write(recvbuf, bytes_read);
							
							recvbuf[0] = '\0';
						}

						to_send = "250 Requested file action okay, completed.\n";
						send(command_socket, to_send.c_str(), to_send.size(), 0);
					}
				}
				else
				{
					std::cout << "Error during file creation" << std::endl;
					to_send = "451 Requested action aborted: local error in processing\n";
					std::cout << "Answered: " << to_send << std::endl;
					send(command_socket, to_send.c_str(), to_send.size(), 0);
					continue;
				}
			}
			else if (msg.substr(0, 4) == "RETR")
			{
				// Now, make sure PORT has been called
				if (data_socket == INVALID_SOCKET)
				{
					// Port hasn't been set
					std::cout << "Attempted to RETR before porting." << std::endl;
					to_send = "451 Requested action aborted: local error in processing\n";
					std::cout << "Answered: " << to_send << std::endl;
					send(command_socket, to_send.c_str(), to_send.size(), 0);
					continue;
				}

				// get file
				std::string fileName = msg.substr(5);

				// Take out those spaces
				fileName = Trim(fileName);

				File file_to_read = currentWorkingDirectory.getChildFile(fileName);

				if (!file_to_read.existsAsFile())
				{
					// File not found
					std::cout << "Error opening source file: |" << msg.substr(5) << "|" << std::endl;
					to_send = "550 Requested action not taken.\n";
					std::cout << "Answered: " << to_send << std::endl;
					send(command_socket, to_send.c_str(), to_send.size(), 0);
					continue;
				}

				FileInputStream input_stream(file_to_read);

				//tell the client that data transfer is about to happen
				to_send = "125 Data connection already open; transfer starting.\n";
				std::cout << "Answered: " << to_send << std::endl;
				send(command_socket, to_send.c_str(), to_send.size(), 0);

				int bytes_to_read = file_to_read.getSize();

				while (bytes_to_read > 0)
				{
					if (bytes_to_read > recvbuflen)
					{
						input_stream.read(recvbuf, recvbuflen);
						send(data_socket, recvbuf, recvbuflen, 0);
						bytes_to_read -= recvbuflen;
					}
					else
					{
						input_stream.read(recvbuf, bytes_to_read);
						send(data_socket, recvbuf, bytes_to_read, 0);
						bytes_to_read = 0;
					}
				}

				closesocket(data_socket);
				data_socket = INVALID_SOCKET;

				// done
				to_send = "250 Requested file action okay, completed.\n";
				std::cout << "Answered: " << to_send << std::endl;
				send(command_socket, to_send.c_str(), to_send.size(), 0);

			}
			else
			{
				// Unimplemented command
				to_send = "502 Command not implemented.\n";
				std::cout << "Answered: " << to_send << std::endl;
				send(command_socket, to_send.c_str(), to_send.size(), 0);
			}
		}
		else if (iResult == 0) {
			printf("Connection closing...\n");
		}
		else {
			printf("recv failed with error: %d\n", WSAGetLastError());
			closesocket(command_socket);
			WSACleanup();
			return;
		}

	} while (iResult > 0);

	// shutdown the connection since we're done
	iResult = shutdown(command_socket, SD_SEND);
	if (iResult == SOCKET_ERROR) {
		printf("shutdown failed with error: %d\n", WSAGetLastError());
		closesocket(command_socket);
	}

	closesocket(command_socket);
}

int __cdecl main(void)
{
	WSADATA wsaData;
	int iResult;

	SOCKET listen_socket = INVALID_SOCKET;
	SOCKET command_socket = INVALID_SOCKET;

	// Initialize Winsock
	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0) {
		printf("WSAStartup failed with error: %d\n", iResult);
		return 1;
	}

	listen_socket = getSocketFD(DEFAULT_PORT, true, "");

	while (true)
	{
		// Accept a client socket
		command_socket = accept(listen_socket, NULL, NULL);
		if (command_socket == INVALID_SOCKET) {
			printf("accept failed with error: %d\n", WSAGetLastError());
			closesocket(listen_socket);
			WSACleanup();
			continue;
		}

		std::thread(working_thread, command_socket).detach();
	}
}