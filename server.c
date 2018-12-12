#include "chat.h"

#include <sys/shm.h>
#include <sys/ipc.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/sem.h>

#define MAXMSG 500
#define MAXUSER 500
#define RW 0
#define MUTEX 1
#define W 2
#define COUNT 3
#define FILESEM 4

union semun{
	int val;
	struct semid_ds *buf;
	unsigned short *array;
};

typedef struct _space{
	int length;
	Message message[MAXMSG];
}Space;


Space *space;
int clntSock;
int servSock;
char client_ip[20];
int shmid;
int semid;

int initSock(int port,int addr);
void do_server();
void readFrom();
void writeTo();
void getunread();
void exitfunc(int signal);
void waitchild(int signal);
int init_sem(int rw,int mutex,int w,int count,int file);
int P(int type);
int V(int type);
int sem_setval(int type,int value);
int cRegister(User user);
int cLogin(User user);
int loadHistory();

int main(void) {
	struct in_addr client_addr;
	int len;
	char *addr;
	signal(SIGINT, exitfunc);
	signal(SIGCHLD, waitchild);
	servSock = initSock(MYPORT, INADDR_ANY);

	shmid = shmget(IPC_PRIVATE, sizeof(Space), IPC_CREAT | 0660);
	if (shmid == -1) {
		printf("shared memeoy created failed.\n");
		return -1;
	}
	space = (Space*)shmat(shmid, NULL, 0);
	if ((int)space == -1) {
		printf("shared memeoy matched failed.\n");
		return -1;
	}

	semid = semget(IPC_PRIVATE, 5, IPC_CREAT | 0660);
	if (semid == -1) {
		printf("semaphore created failed!\n");
		return -1;
	}

	if (init_sem(1, 1, 1, 0, 1) == -1) {
		printf("semaphore initilize failed!\n");
		return -1;
	}

	len = loadHistory();
	if (len == 0) {
		printf("File \"histmsg.dat\" opened succeed!\n");
		printf("Server has loaded the data from the file!\n");
	}
	else if (len == -1) {
		printf("File \"histmsg.dat\" opened failed!\n");
		return -1;
	}
	else {
		printf("File \"histmsg.dat\" is not exist.\n");
	}

	printf("Wating for connecting......\n");
	while (1) {
		clntSock = accept(servSock, NULL, NULL);
		if (clntSock != -1) {
			len = sizeof(client_addr);
			getpeername(clntSock, (struct sockaddr*)&client_addr, &len);
			strcpy(client_ip, inet_ntoa(client_addr));

			printf("Connect succeed!\n");
			printf("Client ip:%s\n", client_ip);

			if (fork() == 0) {
				do_server();
			}
			else {
				close(clntSock);
				strcpy(client_ip, "");
			}
		}
		else printf("connect failed!\n");
	}

	return 0;
}

int initSock(int port, int addr) {
	struct sockaddr_in server_addr;
	int servSock;

	servSock = socket(AF_INET, SOCK_STREAM, 0);
	if (servSock == -1)return -1;

	server_addr.sin_port = htons(port);
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = htonl(addr);

	if (bind(servSock, (struct sockaddr*)&server_addr, sizeof(server_addr)) != 0)
		return -1;
	if (listen(servSock, 5) != 0)
		return -1;
	return servSock;
}

void readFrom() {
	int read_byte;
	char str[141];
	int msglength;
	Packet packet;
	Kind kind;
	Data data;
	while (1) {
		read_byte = read(clntSock, &packet, sizeof(Packet));
		if (read_byte == -1) {
			printf("Client\"%s\" reads error!\n", client_ip);
			return;
		}
		else {
			parse_packet(packet, &kind, &data);

			if (kind == enum_chat) {
				P(W);
				P(RW);
				msglength = space->length%MAXMSG;

				space->message[msglength].id = space->length;
				strcpy(space->message[msglength].str, data.message.str);

				space->length++;
				printf("Client\"%s\" has writed the message %d.\n", client_ip, msglength);

				V(RW);
				V(W);
			}
			else if (kind == enum_logout) {
				printf("Client\"%s\" signs out!\n", client_ip);
				return;
			}
			else {
				printf("the type of the packet reveived is error!\n");
				return;
			}
		}
	}
}

void writeTo() {
	int msglength;
	msglength = space->length;
	int count;
	Packet packet;

	while (1) {
		if (msglength < space->length) {
			P(W);
			P(MUTEX);
			if ((count = semctl(semid, COUNT, GETVAL)) == 0)
				P(RW);
			if (sem_setval(COUNT, count + 1) == -1)
				printf("semaphore set value failed!\n");
			V(MUTEX);
			V(W);

			for (; msglength < space->length; msglength++) {
				if (build_packet(&packet, enum_chat, space->message[msglength%MAXMSG]) == -1) {
					printf("fail to build the packet!\n");
					return;
				}
				write(clntSock, &packet, sizeof(Packet));
			}

			P(MUTEX);
			count = semctl(semid, COUNT, GETVAL);
			if (sem_setval(COUNT, count - 1) == -1)
				printf("semaphore set value failed!\n");
			if (semctl(semid, COUNT, GETVAL) == 0)
				V(RW);
			V(MUTEX);
		}
		sleep(1);
	}
}

void getunread() {
	char lastid[10];
	int fromid;
	int temp;
	Packet packet;

	read(clntSock, lastid, 10);
	for (fromid = atoi(lastid) + 1, temp = fromid; fromid < space->length; fromid++) {
		if (build_packet(&packet, enum_chat, space->message[fromid%MAXMSG]) == -1) {
			printf("fail to build the packet!\n");
			return;
		}
		write(clntSock, &packet, sizeof(packet));
	}
	if (temp < fromid)printf("Client\"%s\" has obtained the unread message between %d to %d.\n", client_ip, temp, fromid - 1);
}

int cRegister(User user) {
	int fd;
	int usernum;
	User userinfo[MAXUSER];
	int i;
	Packet packet;

	fd = open("userinfo.dat", O_RDWR | O_CREAT, 0660);
	if (fd == -1) {
		printf("file \"userinfo.dat\" opened failed!\n");
		return -1;
	}
	P(FILESEM);

	i = read(fd, &usernum, sizeof(int));
	if (i == 0) {
		usernum = 1;
		write(fd, &usernum, sizeof(int));
		write(fd, &user, sizeof(User));
		if (build_packet(&packet, enum_regist, user) == -1) {
			printf("fail to build the packet!\n");
			return -1;
		}
		write(clntSock, &packet, sizeof(Packet));
		printf("Client\"%s\" regists succeed with the account \"%s\".\n", client_ip, user.account);
	}
	else {
		read(fd, userinfo, MAXUSER * sizeof(User));
		for (i = 0; i < usernum; i++) {
			if (!strcmp(userinfo[i].account, user.account)) {
				strcpy(user.account, "");
				if (build_packet(&packet, enum_regist, user) == -1) {
					printf("fail to build the packet!\n");
					return -1;
				}
				write(clntSock, &packet, sizeof(Packet));
				printf("Client\"%s\" regists failed with the repeting account.\n", client_ip);
				close(fd);
				V(FILESEM);
				return -1;
			}
		}
		usernum++;
		strcpy(userinfo[i].account, user.account);
		strcpy(userinfo[i].password, user.password);

		lseek(fd, 0, SEEK_SET);
		write(fd, &usernum, sizeof(int));
		write(fd, userinfo, sizeof(User)*MAXUSER);

		if (build_packet(&packet, enum_regist, user) == -1) {
			printf("fail to build the packet!\n");
			return -1;
		}
		write(clntSock, &packet, sizeof(Packet));
		printf("Client\"%s\" regists succeed with the account \"%s\".\n", client_ip, user.account);
	}
	close(fd);
	V(FILESEM);
	return 0;
}

int cLogin(User user) {
	int fd;
	int usernum;
	User userinfo[MAXUSER];
	int i;
	Packet packet;

	fd = open("userinfo.dat", O_RDWR | O_CREAT, 0660);
	if (fd == -1) {
		printf("file \"userinfo.dat\" opened failed!\n");
		return -1;
	}

	P(FILESEM);

	i = read(fd, &usernum, sizeof(int));
	if (i == 0) {
		strcpy(user.account, "");
		if (build_packet(&packet, enum_login, user) == -1) {
			printf("fail to build the packet!\n");
			return -1;
		}
		write(clntSock, &packet, sizeof(Packet));
		printf("Client\"%s\" logins failed with no account.\n", client_ip);
	}
	else {
		read(fd, userinfo, MAXUSER * sizeof(User));
		for (i = 0; i < usernum; i++) {
			if (!strcmp(userinfo[i].account, user.account) && !strcmp(userinfo[i].password, user.password)) {
				if (build_packet(&packet, enum_login, user) == -1) {
					printf("fail to build the packet!\n");
					return -1;
				}
				write(clntSock, &packet, sizeof(Packet));
				printf("Client\"%s\" logins succeed with the account \"%s\".\n", client_ip, user.account);
				close(fd);
				V(FILESEM);
				return 0;
			}
		}
		strcpy(user.account, "");
		if (build_packet(&packet, enum_login, user) == -1) {
			printf("fail to build the packet!\n");
			return -1;
		}
		write(clntSock, &packet, sizeof(Packet));
		printf("Client\"%s\" logins failed with no account.\n", client_ip);
	}
	close(fd);
	V(FILESEM);
	return -1;
}

void do_server() {
	pthread_t thIDr, thIDw;
	Packet packet;
	Kind kind;
	Data data;

	signal(SIGINT, SIG_DFL);

	read(clntSock, &packet, sizeof(Packet));
	parse_packet(packet, &kind, &data);
	switch (kind) {
	case enum_regist:
		cRegister(data.userinfo);
		return;
	case enum_login:
		if (cLogin(data.userinfo) == -1)
			return;
		break;
	default:
		printf("the type of the packet reveived is error!\n");
		return;
	}

	getunread();

	pthread_create(&thIDr, NULL, (void *)readFrom, NULL);
	pthread_create(&thIDw, NULL, (void *)writeTo, NULL);
	pthread_join(thIDr, NULL);
}

void exitfunc(int signal) {
	int fd;
	if (shmctl(shmid, IPC_RMID, 0) == -1)
		printf("shared memory closed error!\n");
	if (semctl(semid, 0, IPC_RMID, 0) == -1)
		printf("semaphore closed error!\n");
	fd = open("histmsg.dat", O_WRONLY | O_CREAT, 0660);
	if (fd == -1) {
		printf("file \"histmsg.dat\" opened failed!\n");
	}
	else {
		int write_byte;
		write_byte = write(fd, space, sizeof(Space));
		if (write_byte != sizeof(Space)) {
			printf("the length written is incorrect!\n");
		}
		else {
			printf("\nHistory message has stored successfully!\n");
			printf("Server exit!\n");
		}
	}
	if (close(servSock) == -1)
		printf("servSock closed error!\n");
	_exit(0);
}

void waitchild(int signal) {
	wait(NULL);
}

int init_sem(int rw, int mutex, int w, int count, int file) {
	union semun arg;
	int flag;
	arg.array = (unsigned short*)malloc(sizeof(unsigned short) * 5);
	arg.array[RW] = rw;
	arg.array[MUTEX] = mutex;
	arg.array[W] = w;
	arg.array[COUNT] = count;
	arg.array[FILESEM] = file;
	flag = semctl(semid, 0, SETALL, arg);
	free(arg.array);
	return flag;
}

int P(int type) {
	struct sembuf buf;

	buf.sem_num = type;
	buf.sem_op = -1;
	buf.sem_flg = SEM_UNDO;

	return semop(semid, &buf, 1);
}

int V(int type) {
	struct sembuf buf;

	buf.sem_num = type;
	buf.sem_op = 1;
	buf.sem_flg = SEM_UNDO;

	return semop(semid, &buf, 1);
}

int sem_setval(int type, int value) {
	union semun arg;
	arg.val = value;
	return semctl(semid, type, SETVAL, arg);
}

int loadHistory() {
	int fd;
	int read_byte;
	fd = open("histmsg.dat", O_RDONLY);
	if (fd == -1)
		return -2;
	read_byte = read(fd, space, sizeof(Space));
	close(fd);
	if (read_byte == sizeof(Space))
		return 0;
	else
		return -1;
}
