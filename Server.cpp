#include "Server.h"

int socketsCount = 0;

//-------------------------RUN,MESSAGES AND SOCKET MAINTAINE FUNCTIONS--------------------------//

// This function gets client's requests and answer them.
void runServer()
{

	struct SocketState sockets[MAX_SOCKETS] = { 0 };
	WSAData wsaData;
	if (NO_ERROR != WSAStartup(MAKEWORD(2, 2), &wsaData))
	{
		cout << "Time Server: Error at WSAStartup()\n";
		return;
	}
	SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (INVALID_SOCKET == listenSocket)
	{
		cout << "Time Server: Error at socket(): " << WSAGetLastError() << endl;
		WSACleanup();
		return;
	}
	sockaddr_in serverService;
	serverService.sin_family = AF_INET;
	serverService.sin_addr.s_addr = INADDR_ANY;
	serverService.sin_port = htons(TIME_PORT);
	if (SOCKET_ERROR == bind(listenSocket, (SOCKADDR*)&serverService, sizeof(serverService)))
	{
		cout << "Time Server: Error at bind(): " << WSAGetLastError() << endl;
		closesocket(listenSocket);
		WSACleanup();
		return;
	}
	if (SOCKET_ERROR == listen(listenSocket, 5))
	{
		cout << "Time Server: Error at listen(): " << WSAGetLastError() << endl;
		closesocket(listenSocket);
		WSACleanup();
		return;
	}
	addSocket(listenSocket, LISTEN, sockets);

	// Accept connections and handles them one by one.
	while (true)
	{
		fd_set waitRecv;
		FD_ZERO(&waitRecv);
		for (int i = 0; i < MAX_SOCKETS; i++)
		{
			if ((sockets[i].recv == LISTEN) || (sockets[i].recv == RECEIVE))
				FD_SET(sockets[i].id, &waitRecv);
		}

		fd_set waitSend;
		FD_ZERO(&waitSend);
		for (int i = 0; i < MAX_SOCKETS; i++)
		{
			if (sockets[i].send == SEND)
				FD_SET(sockets[i].id, &waitSend);
		}
		int nfd;
		nfd = select(0, &waitRecv, &waitSend, NULL, NULL);
		if (nfd == SOCKET_ERROR)
		{
			cout << "Time Server: Error at select(): " << WSAGetLastError() << endl;
			WSACleanup();
			return;
		}
		checkSocketTimeout(sockets);//checks each round if sockets timedout
		for (int i = 0; i < MAX_SOCKETS && nfd > 0; i++)
		{
			if (FD_ISSET(sockets[i].id, &waitRecv))
			{
				nfd--;
				switch (sockets[i].recv)
				{
				case LISTEN: acceptConnection(i, sockets); break;

				case RECEIVE: receiveMessage(sockets, i); break;
				}
			}
		}
		for (int i = 0; i < MAX_SOCKETS && nfd > 0; i++)
		{
			if (FD_ISSET(sockets[i].id, &waitSend))
			{
				nfd--;
				switch (sockets[i].send)
				{
				case SEND:
					sendMessage(i, sockets);
					resetSocketAfterSend(sockets, i);
					break;
				}
			}
		}
	}
}

//This function add sockets
bool addSocket(SOCKET id, eReceiveState what, SocketState* sockets)
{
	for (int i = 0; i < MAX_SOCKETS; i++)
	{
		if (sockets[i].recv == EMPTY_R)
		{
			sockets[i].id = id;
			sockets[i].recv = what;
			sockets[i].send = IDLE;
			if (sockets[i].buffer != NULL)
				strcpy(sockets[i].buffer, "\0");
			socketsCount++;
			return (true);
		}
	}
	return (false);
}

//This function remove socket
void removeSocket(int index, SocketState* sockets)
{
	sockets[index].recv = EMPTY_R;
	sockets[index].send = EMPTY_S;
	socketsCount--;
}

//This function responsible for accepting connections
void acceptConnection(int index, SocketState* sockets)
{
	SOCKET id = sockets[index].id;
	struct sockaddr_in from; // Address of sending partner
	int fromLen = sizeof(from);

	SOCKET msgSocket = accept(id, (struct sockaddr*)&from, &fromLen);
	if (INVALID_SOCKET == msgSocket)
	{
		cout << "Time Server: Error at accept(): " << WSAGetLastError() << endl;
		return;
	}
	cout << "Time Server: Client " << inet_ntoa(from.sin_addr) << ":" << ntohs(from.sin_port)
		<< " is connected." << endl;

	//
	// Set the socket to be in non-blocking mode.
	//
	unsigned long flag = 1;
	if (ioctlsocket(msgSocket, FIONBIO, &flag) != 0)
	{
		cout << "Time Server: Error at ioctlsocket(): " << WSAGetLastError() << endl;
	}

	if (addSocket(msgSocket, RECEIVE, sockets) == false)
	{
		cout << "\t\tToo many connections, dropped!\n";
		closesocket(id);
	}
	return;
}

//This function responsible for sending the correct response to client
void sendMessage(int index, SocketState* sockets)
{
	int bytesSent = 0;
	string response;
	SOCKET msgSocket = sockets[index].id;
	response.clear();

	if (sockets[index].headers.type == "GET")
	{
		response.append(GET_METHOD(sockets[index]));
	}
	if (sockets[index].headers.type == "OPTIONS")
	{
		response.append(Option_Method(Ok, sockets[index]));
	}
	if (sockets[index].headers.type == "PUT")
	{
		response.append(PUT_METHOD(sockets[index]));
	}
	if (sockets[index].headers.type == "POST")
	{
		response.append(POST_METHOD(sockets[index]));
	}
	if (sockets[index].headers.type == "TRACE")
	{
		// Enter HERE TRACE method logic
		response.append(TRACE_METHOD(sockets[index]));
	}
	if (sockets[index].headers.type == "DELETE")
	{
		response.append(DELETE_METHOD(sockets[index]));
	}
	if (sockets[index].headers.type == "HEAD")
	{
		response.append(HEAD_METHOD(sockets[index]));
	}

	bytesSent = send(msgSocket, response.data(), (int)response.length(), 0);
	if (SOCKET_ERROR == bytesSent)
	{
		cout << "Time Server: Error at send(): " << WSAGetLastError() << endl;
		return;
	}

	cout << "\n---------\nTime Server: Sent: " << bytesSent << "\\" << (int)response.length()
		<< " bytes of \"" << response.data() << "\" message.\n";

	if (sockets[index].headers.connection == "close")
	{
		resetSocketAfterSend(sockets, index);
		closesocket(msgSocket);
		removeSocket(index, sockets);
	}

	sockets[index].send = IDLE;
}

//This function is main prosses of reading from sockets thier content
void receiveMessage(SocketState* socketArr, int index)
{
	SOCKET msgSocket = socketArr[index].id;

	eSocketBufferStatus bufferStatus = readFromSocket(&socketArr[index]); // Read data sent to socket

	switch (bufferStatus)
	{
	case ErrorAtRecv:
		cout << "TCP Web Server: Error at recv(): " << WSAGetLastError() << endl;
		closesocket(msgSocket);
		removeSocket(index, socketArr);
		break;

	case EmptyAtStart:
		closesocket(msgSocket);
		removeSocket(index, socketArr);
		break;

	case ConsumedSuccessfully:
		//classifyRequest(socketArr, index); // Read all data from socket, proceed to classify the request
		Divide_Buffer_Into_Headers(socketArr[index].buffer, &socketArr[index].headers);
		socketArr[index].send = SEND;
		break;
	}

	return;
}

//This function reads from socket its content
eSocketBufferStatus readFromSocket(SocketState* socketToReadFrom)
{
	SOCKET msgSocket = socketToReadFrom->id;
	string temp;
	int bytesRecv = 0, bytesRecvTotal = 0;
	socketToReadFrom->buffer = (char*)malloc(sizeof(char) * socketToReadFrom->bufferSize);
	char* buffer = socketToReadFrom->buffer;

	int iterationCount = 0;
	bool isSocketEmpty = false;

	while (!isSocketEmpty)
	{
		bytesRecv = recv(msgSocket, &(socketToReadFrom->buffer[bytesRecvTotal]),
			socketToReadFrom->bufferSize - bytesRecvTotal, 0);

		if (bytesRecv == 0 &&
			iterationCount == 0) // Socket was empty from beginning, should close socket
		{
			return EmptyAtStart;
		}

		if (bytesRecv == SOCKET_ERROR && iterationCount == 0) // Socket error
		{
			return ErrorAtRecv;
		}

		if (bytesRecv == -1) // Finished reading from socket
		{
			isSocketEmpty = true;
		}
		else
		{
			bytesRecvTotal += bytesRecv;

			// Expand buffer if needed
			if (socketToReadFrom->bufferSize <= bytesRecvTotal)
			{
				socketToReadFrom->bufferSize *= 2;

				char* newBuffer =
					(char*)calloc(socketToReadFrom->bufferSize, sizeof(char));
				memcpy(newBuffer, socketToReadFrom->buffer, bytesRecvTotal);
				free(socketToReadFrom->buffer);

				socketToReadFrom->buffer = newBuffer;
			}
			//cout << "\nMSG RCVD: \n" << socketToReadFrom->buffer << endl;
		}
		iterationCount++;
	}
	temp.assign(socketToReadFrom->buffer).resize(bytesRecvTotal);
	free(socketToReadFrom->buffer);
	socketToReadFrom->buffer = (char*)malloc(sizeof(char) * (bytesRecvTotal+1));
	memcpy(socketToReadFrom->buffer, temp.data(), bytesRecvTotal);
	socketToReadFrom->buffer[bytesRecvTotal] = '\0';
	socketToReadFrom->bufferLength = bytesRecvTotal; // Save message length
	socketToReadFrom->timeSinceLastByte = 0;
	return ConsumedSuccessfully;
}

//This function checks if sockets timedout
void checkSocketTimeout(SocketState* socketArr)
{
	clock_t now = clock();
	
	for (int i = 1; i < MAX_SOCKETS; i++)
	{
		float s_result = socketArr[i].timeSinceLastByte / CLOCKS_PER_SEC;
		float e_result = now / CLOCKS_PER_SEC;
		//if (now - socketArr[i].timeSinceLastByte > MAX_TIMEOUT_SECONDS && socketArr[i].timeSinceLastByte > 0)
		if (((e_result - s_result) > MAX_TIMEOUT_SECONDS) && socketArr[i].timeSinceLastByte >0)
		{
			printf("Socket %d Closed\n", socketArr[i].id);
			socketArr[i].timeSinceLastByte = 0;
			socketArr[i].id = 0;
			removeSocket(i, socketArr);
			closesocket(socketArr[i].id);
			
		}
	}
}


//This function devides the input into headrs and saves it
void Divide_Buffer_Into_Headers(char* msg_received, Msg_Rcvd_Headers* headers)
{
	string time_port = TIME_PORT_STR;
	char msg_copy[BUFFMAXLEN];
	strcpy(msg_copy, msg_received);
	//printf("%s", msg_received);
	char* token = strtok(msg_copy, "- ");
	headers->type = token;
	token = strtok(NULL, "- ");
	headers->URL = token;
	headers->raw_msg = msg_received;
	string buffer(msg_received);

	printf("\nMessage Received: \n%s \n-----------\n", buffer.data());

	size_t accept_id = buffer.find("Accept:") + 7; //saves location after the 'accept:'
	headers->accept = strtok(&msg_received[accept_id], " \r\n");
	size_t host_id = buffer.find("Host:") + 5;
	headers->host = strtok(&msg_received[host_id], " \r\n");
	size_t connection_id = buffer.find("Connection:") + 11;
	headers->connection = strtok(&msg_received[connection_id], " \r\n");
	/*size_t cont_type_id = buffer.find("Content-Type:") + 13;
	headers->Content_Type = strtok(&msg_received[cont_type_id], " \r\n");*/
	size_t cont_type_id = buffer.find("Content-Type:");
	if (cont_type_id != string::npos)
		headers->Content_Type = strtok(&msg_received[cont_type_id + 13], " \r\n");
	else
		headers->Content_Type = "text";
	size_t cont_len_id = buffer.find("Content-Length:") + 15;
	headers->content_len = strtok(&msg_received[cont_len_id], " \r\n");
	size_t body_id = buffer.find(string(CRLF) + CRLF) + 4;
	headers->body.assign(&msg_received[body_id]);
	size_t lang_id = buffer.find("?lang=");
	if (lang_id != string::npos)
		headers->language = strtok(&msg_received[lang_id + 6], " \r\n");
	else
		headers->language.clear();
	size_t file_id = buffer.find(time_port + "/ ");
	if (file_id != string::npos)
		headers->file_name.clear();
	else {
		size_t file_id = buffer.find(time_port + "/");
		headers->file_name = strtok(&msg_received[file_id + time_port.length() + 1], " ?\r\n");
	}
	

}

//-------------------------METHODS IMPLEMENTAION--------------------------//

//OPTIION method implementation
string Option_Method(int status, SocketState sockets)
{
	string response;
	time_t timer;
	//Response message
	Commonheaders(response, status, sockets, false);
	response.append("Allow: OPTIONS, HEAD, GET, POST, PUT, DELETE, TRACE \r\n")
		.append("Content-Lenght: 0\r\n")
		.append("Content-Type: text/html; charset=utf-8\r\n")
		.append("\r\n");

	return response;
}

//GET method implementation
string GET_METHOD(SocketState sockets)
{
	int status;
	string response;
	//Functionality
	status = GetFile(sockets.headers.file_name, sockets.headers.body, sockets.headers.language);
	//Response message
	if (status == Ok)
	{
		Commonheaders(response, status, sockets, false);
		response.append("Content-Location: ")
			.append(GetCurrDir(sockets.headers.file_name))
			.append("\r\n")
			.append("Content-Type: ")
			.append(sockets.headers.Content_Type)
			.append(";charset = utf - 8\r\n")
			.append("Content-Length: ")
			.append(to_string(sockets.headers.body.length()))
			.append("\r\n")
			.append("\r\n")
			.append(sockets.headers.body.data())
			.append("\r\n");
		return response;
	}
	else
	{
		Commonheaders(response, status, sockets, false);
		response.append("\r\n");
		return response;
	}
}

//POST method implementation
string POST_METHOD(SocketState sockets)
{
	int status = Responses::Ok;
	string response;
	string temp("Body was printed to server!");
	//Functionality
	//is to post to the server the body of this message
	cout << "\nPOST request received, body is:\n" << sockets.headers.body << endl;
	//Response message

	Commonheaders(response, status, sockets, false);
	response
		.append("Content-Type: ")
		.append(sockets.headers.Content_Type)
		.append(";charset = utf - 8\r\n")
		.append("Content-Length: ")
		.append(to_string(temp.length()))
		.append("\r\n")
		.append("\r\n")
		.append(temp)
		.append("\r\n");
	return response;

}

//PUT method implementation
string PUT_METHOD(SocketState sockets)
{
	int status;
	string response;
	//Functionality
	status = CreatNewFile(sockets.headers.file_name, sockets.headers.body, sockets.headers.language);
	//Response message
	if (status == Ok || status == Created)
	{
		Commonheaders(response, status, sockets, false);
		response.append("Content-Location: ")
			.append(GetCurrDir(sockets.headers.file_name))
			.append("\r\n")
			.append("Content-Type: ")
			.append(sockets.headers.Content_Type)
			.append(";charset = utf - 8\r\n")
			.append("\r\n");
		return response;
	}
	else
	{
		Commonheaders(response, status, sockets, false);
		response.append("\r\n");
		return response;
	}
}

//Head method implementation
string HEAD_METHOD(SocketState sockets)
{
	int status;
	string response;
	time_t timer;
	string full_file_name(sockets.headers.file_name);
	//Functionality
	if (!sockets.headers.language.empty())
	{
		full_file_name.resize(full_file_name.find("."));
		full_file_name.append("-").append(sockets.headers.language).append(".txt");
		status = IsExist(full_file_name);
	}
	else
		status = IsExist(sockets.headers.file_name);
	//Response message
	if (status == Ok)
	{
		Commonheaders(response, status, sockets, false);
		response.append("Last Modified: ").append(GetLastModifiedDate(full_file_name)).append("\r\n")
			.append("Content-Length: ").append(to_string(FileLenght(full_file_name))).append("\r\n");
		if (!sockets.headers.language.empty())
			response.append("Langauge: ").append(sockets.headers.language).append("\r\n");
			response.append("Content-Type: ").append(sockets.headers.Content_Type).append(";charset = utf - 8\r\n").append("\r\n");
		return response;
	}
	else
	{
		Commonheaders(response, status, sockets, false);
		response.append("\r\n");
		return response;
	}
}

//Delete method implementation
string DELETE_METHOD(SocketState sockets)
{
	int status;
	string response;
	time_t timer = 0;
	//Functionality
	status = Delete_File(sockets.headers.file_name);
	//Response message
	if (status == Ok)
	{
		string temp;
		temp.assign("File ")
			.append(sockets.headers.file_name)
			.append(" deleted \r\n")
			.append("\r\n");

		Commonheaders(response, status, sockets, false);
		response.append("Content-Length: ")
			.append(to_string(temp.length()))
			.append("\r\n")
			.append("\r\n")
			.append(temp)
			.append("\r\n")
			.append("\r\n");
		return response;
	}
	else
	{
		Commonheaders(response, status, sockets, false);
		response.append("\r\n");
		return response;
	}
}

string TRACE_METHOD(SocketState sockets)
{ //returns the body message ! //i wiil returns the body into the response message using append/strcpy or shit like this
	string response;
	Commonheaders(response, (int)Ok, sockets, true);
	response.append(sockets.headers.raw_msg).append("\r\n").append("\r\n");

	return response;
}

//This function is common headers for all commands
void Commonheaders(string& response, int status, SocketState sockets, bool from_trace)
{
	time_t timer;
	time(&timer);
	//Getting the server name
	string servername = GetServerName();
	response.assign("HTTP/1.1 ")
		.append(to_string(status).data())
		.append(Status_code_converter(status))
		.append("\r\n")
		.append("Date: ")
		.append(ctime(&timer))
		.append("Host Name: ")
		.append(sockets.headers.host)
		.append("\r\n")
		.append("Server: ")
		.append(servername.data())
		.append(" (Windows)")
		.append("\r\n");
	if (from_trace == false)
	{
		response.append("Connection: ")
			.append(sockets.headers.connection)
			.append("\r\n"); //append here the connection status
	}
}

//This function is converts enum statuses
const char* Status_code_converter(int status)
{
	switch (status)
	{
	case Ok: return " Ok"; break;
	case Created: return " Created"; break;
	case Accepted: return " Accepted"; break;
	case No_Content: return " No_Content"; break;
	case Nor_Autorized: break; return " Not Autorized";
	case Not_found: return " Not found"; break;
	case Request_Timeout: return " Request Timeout"; break;
	default: //Forbbiden
		return " Forbbiden";
		break;
	}
}

//-------------------------FILES--------------------------//

//This function is checks if file exist
int IsExist(string filename)
{
	FILE* fp;
	fp = fopen(filename.data(), "r");
	if (fp == nullptr)
		return Not_found;
	else
	{
		fclose(fp);
		return Ok;
	}
}

//This function delete a file from current directory.
int Delete_File(string filename)
{
	int status;
	status = IsExist(filename);
	if (status == Not_found)
		return status;
	else
	{
		status = remove(filename.data());
		if (status != 0)
		{
			return Accepted;
		}
		else
			return Ok;
	}
}

//This function creats a file if dont exist in dictionary ,and if so will trunk its content and write into it at current directory.
int CreatNewFile(string filename, string body, string language)
{
	FILE* fp;
	int status = 0;
	string full_file_name(filename);
	if (!language.empty())
	{
		full_file_name.resize(full_file_name.find("."));
		full_file_name.append("-").append(language).append(".txt");
		fp = fopen(full_file_name.c_str(), "r+");
	}
	else
		fp = fopen(full_file_name.c_str(), "r+");
	
	if (fp == nullptr)
	{
		status = Created;
		fp = fopen(full_file_name.data(), "w");
		if (fp == nullptr)
			return Not_found;
		fprintf(fp, "%s", body.data());
		fclose(fp);
	}
	else
	{ //the file is already exist
		status = Ok;
		fprintf(fp, "%s", body.data());
	}

	return status;
}

//This function get the last time file was modified
string GetLastModifiedDate(string filename)
{
	string modifiedtime;
	struct stat filestat;
	stat(filename.data(), &filestat);
	modifiedtime.append(ctime(&filestat.st_mtime));
	modifiedtime[modifiedtime.length() - 1] = ' ';
	return modifiedtime;
}

//This function get the file size
int FileLenght(string filename)
{
	FILE* fp;
	fp = fopen(filename.data(), "r");
	assert(fp);
	fseek(fp, 0L, SEEK_END);
	int sz = ftell(fp);
	fclose(fp);
	return sz;
}

//This function gets file name and  extract from it its content
int GetFile(string& filename, string& body, string& language)
{
	FILE* fp;
	string full_file_name(filename);
	int status = 0, x;

	if (!language.empty())
	{
		full_file_name.resize(full_file_name.find("."));
		full_file_name.append("-").append(language).append(".txt");
		fp = fopen(full_file_name.c_str(), "r+");
	}
	else
		fp = fopen(full_file_name.c_str(), "r+");

	if (fp != nullptr)
	{
		std::ifstream t(full_file_name);
		std::string str;
		t.seekg(0, std::ios::end);
		body.reserve(t.tellg());
		t.seekg(0, std::ios::beg);
		body.assign((std::istreambuf_iterator<char>(t)), std::istreambuf_iterator<char>());

		if (body.empty() == true)
		{
			status = Responses::No_Content;
			body.assign("file is empty");
		}
		else
			status = Responses::Ok;

		fclose(fp);
	}
	else
		status = Responses::Not_found;


	return status;
}


//--------------------PATHS AND DOMAIN NAME--------------------//

//This function is gets domain name
string GetServerName()
{
	//Getting the server name (move it to function asap!!)
	TCHAR m_szComputerName[MAX_COMPUTERNAME_LENGTH + 1];
	DWORD dwNameSize = MAX_COMPUTERNAME_LENGTH + 1;
	GetComputerName(m_szComputerName, &dwNameSize);
	wstring help(&m_szComputerName[0]);
	string servername(help.begin(), help.end());
	return servername;
}

//This function gets the directory path
string GetCurrDir(string filename)
{
	TCHAR buffer[MAX_PATH] = { 0 };
	GetModuleFileName(NULL, buffer, MAX_PATH);
	wstring helper(&buffer[0]);                //convert to wstring
	string Path(helper.begin(), helper.end()); //and convert to string.
	Path.resize(Path.size() - STRINGCUT);
	Path.append(filename);
	return Path;
}

//This function resets sockets
void resetSocketAfterSend(SocketState* socketArr, int index)
{
	SocketState* socketToReset = &socketArr[index];
	socketToReset->send = eSendState::IDLE;
	socketToReset->bufferLength = 0;
	socketToReset->bufferSize = INIT_SOCKET_BUF_SIZE;
	socketToReset->timeSinceLastByte = clock();
	if (socketArr[index].buffer != NULL)
		free(socketArr[index].buffer);
}

