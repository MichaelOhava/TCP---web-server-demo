#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#ifndef _USER
#define _USER
#include <iostream>
#pragma comment(lib, "Ws2_32.lib")
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <sys/stat.h>
#include <time.h>
#include <winsock2.h>
#include <fstream>
#include <streambuf>

#define MAX_TIMEOUT_SECONDS 120
#define FAILURE -1
#define BUFFMAXLEN 500
#define MAX_SOCKETS 60
#define TIME_PORT 27016
#define TIME_PORT_STR "27016"
#define INIT_SOCKET_BUF_SIZE 526
#define CRLF "\r\n"
#define STRINGCUT 20
using namespace std;

enum eSocketBufferStatus
{
	EmptyAtStart,
	ErrorAtRecv,
	ConsumedSuccessfully
};
enum eSendState
{
	EMPTY_S,
	IDLE,
	SEND
};
enum Times
{
	SEND_TIME = 1,
	SEND_SECONDS = 2
};
enum Responses
{
	Ok = 200,
	Created = 201,
	Accepted = 202,
	No_Content = 204,
	Nor_Autorized = 401,
	Not_found = 404,
	Request_Timeout = 408,
	Forbbiden = 403
};
enum Connection_Status
{
	Keep_Alive,
	Closed
};
enum eReceiveState
{
	EMPTY_R,
	RECEIVE,
	LISTEN
};
enum eReqMethod
{
	OPTIONS,
	GET,
	PUT,
	POST,
	mDELETE,
	TRACE,
	HEAD,
	UNSUPPORTED
};
enum eReqError
{
	UnsupportedHeader,
	UnsupportedMethod,
	DirectoryNotFound,
	ResourceNotFound,
	NoError
};

typedef struct Msg_Rcvd_Headers
{
	string type;
	string URL;
	string Content_Type;
	string accept;
	string host;
	string connection;
	string language;
	string file_name;
	string content_len;
	string body;
	string raw_msg;
};

typedef struct SocketState
{
	SOCKET id; // Socket handle
	eReceiveState recv;
	eSendState send;
	eReqMethod ReqMethod; // Request type
	char* buffer;
	int bufferSize = INIT_SOCKET_BUF_SIZE; //PHYSICAL SIZE
	int bufferLength;                      //logical size
	clock_t timeSinceLastByte;              // Time since last byte read
	Msg_Rcvd_Headers headers;
} SocketState;

const char* Status_code_converter(int status); //להתאים את סטטוס הפעולה לקראת שליחת התגובה
bool addSocket(SOCKET id, eReceiveState what, SocketState* sockets);
void removeSocket(int index, SocketState* sockets);
void acceptConnection(int index, SocketState* sockets);
void receiveMessage(SocketState* socketArr, int index);
void sendMessage(int index, SocketState* sockets);
void checkSocketTimeout(SocketState* socketArr);
string Option_Method(int status, SocketState sockets);
string GET_METHOD(SocketState sockets);
string POST_METHOD(SocketState sockets);
string PUT_METHOD(SocketState sockets);
string HEAD_METHOD(SocketState sockets);
string DELETE_METHOD(SocketState sockets);
string TRACE_METHOD(SocketState sockets);
int IsExist(string filename);
int Delete_File(string filename);
int CreatNewFile(string filename, string body, string language);
string GetLastModifiedDate(string filename);
int FileLenght(string filename);
string GetCurrDir(string filename);
string GetServerName();
void Divide_Buffer_Into_Headers(char* msg_received, Msg_Rcvd_Headers* headers);
void resetSocketAfterSend(SocketState* socketArr, int index);
eSocketBufferStatus readFromSocket(SocketState* socketToReadFrom);
void Commonheaders(string& response, int status, SocketState sockets, bool from_trace);
int GetFile(string& filename, string& body, string& language);
void runServer();

#endif
