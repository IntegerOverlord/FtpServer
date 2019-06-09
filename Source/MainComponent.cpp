/*
  ==============================================================================

  This is an automatically generated GUI class created by the Projucer!

  Be careful when adding custom code to these files, as only the code within
  the "//[xyz]" and "//[/xyz]" sections will be retained when the file is loaded
  and re-saved.

  Created with Projucer version: 5.4.3

  ------------------------------------------------------------------------------

  The Projucer is part of the JUCE library.
  Copyright (c) 2017 - ROLI Ltd.

  ==============================================================================
*/

//[Headers] You can add your own extra header files here...
//[/Headers]

#undef UNICODE

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <iostream>
#include <thread>

// Need to link with Ws2_32.lib
#pragma comment (lib, "Ws2_32.lib")
// #pragma comment (lib, "Mswsock.lib")

#define DEFAULT_BUFLEN 4096
#define DEFAULT_PORT "21"

#include "MainComponent.h"

void MainComponent::append_text_to_log(std::string str)
{
	const MessageManagerLock mml(Thread::getCurrentThread());

	String current_str = logTextEditor->getText();
	current_str = String(str) + current_str;
	logTextEditor->setText(current_str.substring(0, 5000));
}


// Gets the fd for the socket. Returns -1 on failure
// toBind - whether you bind or connect
// address - address to connect to (not used for bind)
SOCKET MainComponent::getSocketFD(char* s, bool toBind, std::string address)
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
			append_text_to_log("Error: getaddrinfo failed.\n");
			return INVALID_SOCKET;
		}
	}
	else
	{
		if ((status = getaddrinfo(address.c_str(), s, &hints, &servinfo)) != 0)
		{
			append_text_to_log("Error: getaddrinfo failed.\n");
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
			append_text_to_log("Error: setsockopt failed.\n");
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
		append_text_to_log("Error: failed to bind to anything.\n");
		return INVALID_SOCKET;
	}

	//Free up the space since the addrinfo is no longer needed
	freeaddrinfo(servinfo);

	//Try to listen if you're binding
	if (toBind)
	{
		if (listen(sock, SOMAXCONN) == -1)
		{
			append_text_to_log("Error: failed to listen.\n");
			return INVALID_SOCKET;
		}
	}

	// return the fd
	return sock;
}

SOCKET MainComponent::getSocketFD(int p, bool toBind, std::string address)
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

//[MiscUserDefs] You can add your own user definitions and misc code here...
//[/MiscUserDefs]

//==============================================================================

void MainComponent::acception_thread(SOCKET listen_socket)
{
	while (true)
	{
		// Accept a client socket
		SOCKET command_socket = accept(listen_socket, NULL, NULL);
		if (command_socket == INVALID_SOCKET) {
			append_text_to_log("accept failed with error: " + WSAGetLastError());
			closesocket(listen_socket);
			WSACleanup();
			continue;
		}

		threads_running.push_back(std::thread(&MainComponent::working_thread, this, command_socket));
	}
}


void MainComponent::working_thread(SOCKET command_socket)
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
	append_text_to_log(to_send);
	send(command_socket, to_send.c_str(), to_send.length(), 0);

	// Receive until the peer shuts down the connection
	do {

		iResult = recv(command_socket, recvbuf, recvbuflen, 0);
		if (iResult > 0) {
			std::string msg = "";
			for (int x = 0; x < iResult; x++)
			{
				msg.push_back(recvbuf[x]);
			}

			append_text_to_log("Received: " + msg);

			if (msg.substr(0, 4) == "USER")
			{
				// Accept all users because I'm secure like that
				to_send = "230 User logged in, proceed.\n";
				append_text_to_log(to_send);
				send(command_socket, to_send.c_str(), to_send.size(), 0);
			}
			else if (msg.substr(0, 4) == "PASS")
			{
				// Accept all users because I'm secure like that
				to_send = "230 User logged in, proceed.\n";
				append_text_to_log(to_send);
				send(command_socket, to_send.c_str(), to_send.size(), 0);
			}
			else if (msg.substr(0, 4) == "QUIT")
			{
				// Quit
				to_send = "221 Service closing control connection.\n";
				append_text_to_log(to_send);
				send(command_socket, to_send.c_str(), to_send.size(), 0);
			}
			else if (msg.substr(0, 3) == "PWD")
			{
				to_send = "257 \"" + currentWorkingDirectory.getFullPathName().toStdString() + "\\\" is your current location\n";
				append_text_to_log(to_send);
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
				append_text_to_log(to_send);
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

				append_text_to_log("Attempting to open port: " + std::to_string(portNumber) + "\n");
				append_text_to_log("From address: " + address + "\n");

				data_socket = getSocketFD(portNumber, false, address);

				if (data_socket == INVALID_SOCKET)
				{
					// Port open failure
					to_send = "425 Can't open data connection.\n";
					append_text_to_log(to_send);
					send(command_socket, to_send.c_str(), to_send.size(), 0);
					continue;
				}

				// Port open success
				to_send = "200 Command okay.\n";
				append_text_to_log(to_send);
				send(command_socket, to_send.c_str(), to_send.size(), 0);
			}
			else if (msg.substr(0, 4) == "LIST")
			{
				// Now, make sure PORT has been called
				if (data_socket == INVALID_SOCKET)
				{
					// Port hasn't been set
					to_send = "451 Requested action aborted: local error in processing\n";
					append_text_to_log(to_send);
					send(command_socket, to_send.c_str(), to_send.size(), 0);
					continue;
				}

				// Open directory
				File& directory = currentWorkingDirectory;
				std::string files = "";
				if (Trim(msg) == "LIST")
				{
					append_text_to_log("LIST with param");
				}
				else
				{
					append_text_to_log(to_send);
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
				append_text_to_log(to_send);
				send(command_socket, to_send.c_str(), to_send.size(), 0);

				// send
				send(data_socket, files.c_str(), files.length(), 0);
				append_text_to_log("Files list:\n" + files);
				closesocket(data_socket);
				data_socket = INVALID_SOCKET;

				// done
				to_send = "250 Requested file action okay, completed.\n";

				append_text_to_log(to_send);
				send(command_socket, to_send.c_str(), to_send.size(), 0);
			}
			else if (msg.substr(0, 4) == "CDUP")
			{
				currentWorkingDirectory = currentWorkingDirectory.getParentDirectory();
				currentWorkingDirectory.setAsCurrentWorkingDirectory();

				to_send = "250 Okay.\n";
				append_text_to_log(to_send);
				send(command_socket, to_send.c_str(), to_send.size(), 0);
			}
			else if (msg.substr(0, 3) == "CWD")
			{
				currentWorkingDirectory = currentWorkingDirectory.getChildFile(Trim(msg.substr(4)));
				currentWorkingDirectory.setAsCurrentWorkingDirectory();

				to_send = "250 Okay.\n";
				append_text_to_log(to_send);
				send(command_socket, to_send.c_str(), to_send.size(), 0);
			}
			else if (msg.substr(0, 4) == "FEAT")
			{
				to_send = "211-Features:\nUTF8\n211 END\n";
				append_text_to_log(to_send);
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
					append_text_to_log(to_send);
					send(command_socket, to_send.c_str(), to_send.size(), 0);
				}
				else
				{
					to_send = "451 Requested action aborted: local error in processing\n";
					append_text_to_log(to_send);
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
					append_text_to_log(to_send);
					send(command_socket, to_send.c_str(), to_send.size(), 0);
				}
				else
				{
					to_send = "451 Requested action aborted: local error in processing\n";
					append_text_to_log(to_send);
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
					append_text_to_log(to_send);
					send(command_socket, to_send.c_str(), to_send.size(), 0);
				}
				else
				{
					to_send = "451 Requested action aborted: local error in processing\n";
					append_text_to_log(to_send);
					send(command_socket, to_send.c_str(), to_send.size(), 0);
				}

				to_send = "257 Deleted.\n";
				append_text_to_log(to_send);
				send(command_socket, to_send.c_str(), to_send.size(), 0);
			}
			else if (msg.substr(0, 4) == "RNFR")
			{
				std::string name = msg.substr(5);
				rename_from_filename = Trim(name);

				to_send = "250 Okay\n";
				append_text_to_log(to_send);
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
					append_text_to_log(to_send);
					send(command_socket, to_send.c_str(), to_send.size(), 0);
				}
				else
				{
					to_send = "451 Requested action aborted: local error in processing\n";
					append_text_to_log(to_send);
					send(command_socket, to_send.c_str(), to_send.size(), 0);
				}
			}
			else if (msg.substr(0, 4) == "STOR")
			{
				// Now, make sure PORT has been called
				if (data_socket == INVALID_SOCKET)
				{
					// Port hasn't been set
					append_text_to_log("Attempted to STOR before porting.\n");
					to_send = "451 Requested action aborted: local error in processing\n";
					append_text_to_log(to_send);
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
					append_text_to_log("Error during file creation\n");
					to_send = "451 Requested action aborted: local error in processing\n";
					append_text_to_log(to_send);
					send(command_socket, to_send.c_str(), to_send.size(), 0);
					continue;
				}

				closesocket(data_socket);
				data_socket = INVALID_SOCKET;
			}
			else if (msg.substr(0, 4) == "RETR")
			{
				// Now, make sure PORT has been called
				if (data_socket == INVALID_SOCKET)
				{
					// Port hasn't been set
					append_text_to_log("Attempted to RETR before porting.");
					to_send = "451 Requested action aborted: local error in processing\n";
					append_text_to_log(to_send);
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
					append_text_to_log("Error opening source file: " + msg.substr(5));
					to_send = "550 Requested action not taken.\n";
					append_text_to_log(to_send);
					send(command_socket, to_send.c_str(), to_send.size(), 0);
					continue;
				}

				//tell the client that data transfer is about to happen
				to_send = "125 Data connection already open; transfer starting.\n";
				append_text_to_log(to_send);
				send(command_socket, to_send.c_str(), to_send.size(), 0);

				if (image_mode)
				{
					FileInputStream input_stream(file_to_read);

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
				}
				else
				{
					String file_text = file_to_read.loadFileAsString();
					send(data_socket, file_text.getCharPointer(), file_text.length(), 0);
				}

				closesocket(data_socket);
				data_socket = INVALID_SOCKET;

				// done
				to_send = "250 Requested file action okay, completed.\n";
				append_text_to_log(to_send);
				send(command_socket, to_send.c_str(), to_send.size(), 0);

			}
			else
			{
				// Unimplemented command
				to_send = "502 Command not implemented.\n";
				append_text_to_log(to_send);
				send(command_socket, to_send.c_str(), to_send.size(), 0);
			}
		}
		else if (iResult == 0) {
			append_text_to_log("Client thread exiting...\n");
		}
		else {
			append_text_to_log("recv failed with error: " + std::to_string(WSAGetLastError()) + "\n");
			closesocket(command_socket);
			WSACleanup();
			return;
		}

	} while (iResult > 0);

	// shutdown the connection since we're done
	iResult = shutdown(command_socket, SD_SEND);
	if (iResult == SOCKET_ERROR) {
		append_text_to_log("shutdown failed with error: " + std::to_string(WSAGetLastError()) + "\n");
		closesocket(command_socket);
	}

	closesocket(command_socket);
}


MainComponent::MainComponent ()
{
    //[Constructor_pre] You can add your own custom stuff here..
    //[/Constructor_pre]

    label.reset (new Label ("new label",
                            TRANS("Port Number:")));
    addAndMakeVisible (label.get());
    label->setFont (Font (15.00f, Font::plain).withTypefaceStyle ("Regular"));
    label->setJustificationType (Justification::centredLeft);
    label->setEditable (false, false, false);
    label->setColour (TextEditor::textColourId, Colours::black);
    label->setColour (TextEditor::backgroundColourId, Colour (0x00000000));

    label->setBounds (10, 10, 150, 24);

    portNumberEditor.reset (new TextEditor ("new text editor"));
    addAndMakeVisible (portNumberEditor.get());
    portNumberEditor->setMultiLine (false);
    portNumberEditor->setReturnKeyStartsNewLine (false);
    portNumberEditor->setReadOnly (false);
    portNumberEditor->setScrollbarsShown (true);
    portNumberEditor->setCaretVisible (true);
    portNumberEditor->setPopupMenuEnabled (true);
    portNumberEditor->setText (String());

    portNumberEditor->setBounds (120, 10, 150, 24);

    StartStopButton.reset (new TextButton ("new button"));
    addAndMakeVisible (StartStopButton.get());
    StartStopButton->setButtonText (TRANS("Start"));
    StartStopButton->addListener (this);

    StartStopButton->setBounds (10, 50, 150, 24);

    logTextEditor.reset (new TextEditor ("new text editor"));
    addAndMakeVisible (logTextEditor.get());
    logTextEditor->setMultiLine (true);
    logTextEditor->setReturnKeyStartsNewLine (true);
    logTextEditor->setReadOnly (true);
    logTextEditor->setScrollbarsShown (true);
    logTextEditor->setCaretVisible (false);
    logTextEditor->setPopupMenuEnabled (true);
    logTextEditor->setText (String());

    logTextEditor->setBounds (10, 90, 580, 300);


    //[UserPreSize]
    //[/UserPreSize]

    setSize (600, 400);


    //[Constructor] You can add your own custom stuff here..
    //[/Constructor]

	WSADATA wsaData;
	int iResult;
	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0) {

		append_text_to_log("WSAStartup failed with error: " + std::to_string(WSAGetLastError()) + "\n");
	}
}

MainComponent::~MainComponent()
{
    //[Destructor_pre]. You can add your own custom destruction code here..
    //[/Destructor_pre]

    label = nullptr;
    portNumberEditor = nullptr;
    StartStopButton = nullptr;
    logTextEditor = nullptr;

	for (int i = 0; i < threads_running.size(); i++)
	{
		if (threads_running[i].joinable())
		{
			TerminateThread(threads_running[i].native_handle(), 0);
			threads_running[i].join();
		}
	}


    //[Destructor]. You can add your own custom destruction code here..
    //[/Destructor]
}

//==============================================================================
void MainComponent::paint (Graphics& g)
{
    //[UserPrePaint] Add your own custom painting code here..
    //[/UserPrePaint]

    g.fillAll (Colour (0xff323e44));

    //[UserPaint] Add your own custom painting code here..
    //[/UserPaint]
}

void MainComponent::resized()
{
    //[UserPreResize] Add your own custom resize code here..
    //[/UserPreResize]

    //[UserResized] Add your own custom resize handling here..
    //[/UserResized]
}

void MainComponent::buttonClicked (Button* buttonThatWasClicked)
{
    //[UserbuttonClicked_Pre]
    //[/UserbuttonClicked_Pre]

    if (buttonThatWasClicked == StartStopButton.get())
    {
		if (StartStopButton->getButtonText() == "Start")
		{
			threads_running.clear();

			if (portNumberEditor->getText() == "")
			{
				SOCKET listen_socket = getSocketFD(DEFAULT_PORT, true, "");
				threads_running.push_back(std::thread(&MainComponent::acception_thread, this, listen_socket));
			}
			else
			{
				SOCKET listen_socket = getSocketFD(portNumberEditor->getText().getIntValue(), true, "");
				threads_running.push_back(std::thread(&MainComponent::acception_thread, this, listen_socket));
			}

			StartStopButton->setButtonText(TRANS("Stop"));
			portNumberEditor->setReadOnly(true);
			append_text_to_log("FTP Server Started\n");
		}
		else
		{
			for (int i = 0; i < threads_running.size(); i++)
			{
				if (threads_running[i].joinable())
				{
					TerminateThread(threads_running[i].native_handle(), 0);
					threads_running[i].join();
				}
			}

			threads_running.clear();

			StartStopButton->setButtonText(TRANS("Start"));
			portNumberEditor->setReadOnly(false);
			append_text_to_log("FTP Server Stopped\n");
		}
    }

    //[UserbuttonClicked_Post]
    //[/UserbuttonClicked_Post]
}



//[MiscUserCode] You can add your own definitions of your custom methods or any other code here...
//[/MiscUserCode]


//==============================================================================
#if 0
/*  -- Projucer information section --

    This is where the Projucer stores the metadata that describe this GUI layout, so
    make changes in here at your peril!

BEGIN_JUCER_METADATA

<JUCER_COMPONENT documentType="Component" className="MainComponent" componentName=""
                 parentClasses="public Component" constructorParams="" variableInitialisers=""
                 snapPixels="8" snapActive="1" snapShown="1" overlayOpacity="0.330"
                 fixedSize="0" initialWidth="600" initialHeight="400">
  <BACKGROUND backgroundColour="ff323e44"/>
  <LABEL name="new label" id="1ceaccead1f6a7f5" memberName="label" virtualName=""
         explicitFocusOrder="0" pos="10 10 150 24" edTextCol="ff000000"
         edBkgCol="0" labelText="Port Number:" editableSingleClick="0"
         editableDoubleClick="0" focusDiscardsChanges="0" fontname="Default font"
         fontsize="1.5e1" kerning="0" bold="0" italic="0" justification="33"/>
  <TEXTEDITOR name="new text editor" id="38f5ef2546b0b805" memberName="portNumberEditor"
              virtualName="" explicitFocusOrder="0" pos="120 10 150 24" initialText=""
              multiline="0" retKeyStartsLine="0" readonly="0" scrollbars="1"
              caret="1" popupmenu="1"/>
  <TEXTBUTTON name="new button" id="6621cc2a0b7f4f54" memberName="StartStopButton"
              virtualName="" explicitFocusOrder="0" pos="10 50 150 24" buttonText="Start"
              connectedEdges="0" needsCallback="1" radioGroupId="0"/>
  <TEXTEDITOR name="new text editor" id="31a22724ecac2ca3" memberName="logTextEditor"
              virtualName="" explicitFocusOrder="0" pos="10 90 580 300" initialText=""
              multiline="1" retKeyStartsLine="1" readonly="1" scrollbars="1"
              caret="0" popupmenu="1"/>
</JUCER_COMPONENT>

END_JUCER_METADATA
*/
#endif


//[EndFile] You can add extra defines here...
//[/EndFile]

