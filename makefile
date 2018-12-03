program:client server

client:client.c chat.o
	gcc -o client client.c chat.o -lpthread -lsocket -lnsl
server:server.c chat.o
	gcc -o server server.c chat.o -lpthread -lsocket -lnsl
chat.o:chat.h chat.c
	gcc -c chat.c
clean:
	rm *.o server client
