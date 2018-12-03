#ifndef chat_h
#define chat_h

#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdarg.h>

#define MYPORT 8000
#define MAXLEN 140

typedef struct _message{
	int id;
	char str[MAXLEN+1];
}Message;

typedef struct _user{
	char account[20];
	char password[20];
}User;

typedef union _data{
	User userinfo;
	Message message;
}Data;

typedef enum _kind {
	enum_regist, enum_login, enum_logout, enum_chat
}Kind;

typedef struct _packet{
	Kind kind;
	Data data;
}Packet;


int build_packet(Packet *packet,Kind kind,...);
int parse_packet(Packet packet,Kind *kind,Data *data);
#endif
