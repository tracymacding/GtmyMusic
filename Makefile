TARGETS = server client
LIBS = -lpthread -lcrypto
all : $(TARGETS)

CFLASS = -Wall -g

server : main.o command.o
	cc -o $@ main.o command.o $(LIBS)
client : client.o
	cc -o $@ client.o $(LIBS)

.c.o:
	$(CC) -c $(CFLAGS) -o $@ $<

main.o : main.c command.h command.c
command.o : command.h command.c
client.o : client.c

clean:
	rm -rf *.o $(TARGETS)
