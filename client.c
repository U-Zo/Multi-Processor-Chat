#include "chat.h"

int lastmessage;
int client_socket;
char user_account[20];

int initSock(int, char *);
int compare_account(char *, char *);
void menu();

int main(int argc, char *argv[]) {
	pthread_t thIDr, thIDw;
	int fd;
	char str[50];
	int select;
	int flag;

	if (argc != 1) {
		printf("usage:%s\n\n", argv[0]);
		return -1;
	}

	flag = 0;
	while (1) {
		if (flag == 0) {
			menu();

			select = -1;
			do {
				scanf("%d", &select);
			} while (select < 0 || select > 3);

			if (select == 0)
				return 0;
			else {
				Packet packet;
				Kind kind;
				Data data;

				client_socket = initSock(MYPORT, "127.0.0.1");
				if (client_socket == -1) {
					printf("connect error!\n");
					return -1;
				}

				if (select == 1) {
					getUserInfo(enum_regist, &data);
					if (build_packet(&packet, enum_regist, data) == -1) {
						printf("fail to build the packet!\n");
						return -1;
					}

					write(client_socket, &packet, sizeof(Packet));
					read(client_socket, &packet, sizeof(Packet));
					parse_packet(packet, &kind, &data);
					if (kind != enum_regist) {
						printf("the type of the packet received is error!\n");
						return -1;
					}

					if (strcmp(packet.data.userinfo.account, ""))
						printf("Regist succeed.\n");
					else
						printf("Regist failed.\n");
					sleep(1);
				}
				else if (select == 2) {
					getUserInfo(enum_login, &data);
					if (build_packet(&packet, enum_login, data) == -1) {
						printf("fail to build the packet!\n");
						return -1;
					}

					write(client_socket, &packet, sizeof(Packet));
					read(client_socket, &packet, sizeof(Packet));
					parse_packet(packet, &kind, &data);
					if (kind != enum_login) {
						printf("the type of the packet received is error!\n");
						return -1;
					}

					if (strcmp(packet.data.userinfo.account, "")) {
						printf("Login succeed.\n");
						fgets(user_account, 20, stdin);
						strcpy(user_account, packet.data.userinfo.account);
						flag = 1;
					}
					else {
						printf("Login failed.\n");
					}
					sleep(1);
				}
				if (select != 2)close(client_socket);
			}
		}
		else {
			fd = open(user_account, O_RDONLY);
			if (fd != -1) {
				char id[10];
				read(fd, id, 10);
				lastmessage = atoi(id);
				write(client_socket, id, 10);
				close(fd);
			}
			else {
				write(client_socket, "-1", 5);
			}

			printf("\n-----Welcome-----\n");
			pthread_create(&thIDr, NULL, (void*)readFrom, NULL);
			pthread_create(&thIDw, NULL, (void*)writeTo, NULL);
			pthread_join(thIDw, NULL);
			pthread_cancel(thIDr);
			fd = open(user_account, O_WRONLY | O_CREAT, 0660);
			if (fd != -1) {
				char str[10];
				sprintf(str, "%d", lastmessage);
				write(fd, str, 10);
				close(fd);
			}
			else {
				printf("open error!\n");
			}

			close(client_socket);
			flag = 0;
		}
	}
}

void menu() {
	printf("\n");
	printf("1. Sign Up\n");
	printf("2. Sign In\n");
	printf("0. Quit\n");
	printf("Select number : ");
}

int initSock(int port, char *addr) {
	int clntSock;
	int tryTime;
	struct sockaddr_in server_addr;

	clntSock = socket(AF_INET, SOCK_STREAM, 0);
	if (clntSock == -1)
		return -1;

	server_addr.sin_addr.s_addr = inet_addr(addr);
	server_addr.sin_port = htons(port);
	server_addr.sin_family = AF_INET;

	tryTime = 0;
	while (tryTime < 10 && connect(clntSock, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1)
		sleep(1);
	if (tryTime >= 10)
		return -1;
	else
		return clntSock;
}

int compare_account(char *account, char *str) {
	char temp[20];
	char *ptr = temp;

	while ((*ptr++ = *str++) != ':');
	*(ptr - 1) = '\0';

	return
		strcmp(temp, account);
}

int do_order(char *account, char *str) {
	char temp[20];
	char order[20];
	char *ptr = temp;
	char *ptr2 = order;
	Packet packet;
	Message message;
	int i = 0;

	while ((*ptr++ = *str++) != ':');
	*(ptr - 1) = '\0';

	while ((*ptr2++ = *str++) != '\0');
	*(ptr2 - 1) = '\0';

	if (strcmp(temp, "/kick") == 0) {
		if (strcmp(order, account) == 0) {
			printf("You were kicked out.\n");
			build_packet(&packet, enum_logout, message);
			write(client_socket, &packet, sizeof(Packet));
			return -1;
		}
	}

	return 0;
}
