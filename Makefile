output: lab1b-client.c lab1b-server.c
	make client;
	make server;

# debug: lab1b.c
# 	gcc -Wall -Wextra -g lab1b.c -o lab1b

client:  lab1b-client.c test.c
	gcc -Wall -Wextra  lab1b-client.c -o lab1b-client -lz

server: lab1b-server.c test.c
	gcc -Wall -Wextra lab1b-server.c -o lab1b-server -lz


clean:
	rm -f *.o
	rm -f lab1b-client
	rm -f lab1b-server
	rm -f *.gz
	rm -f *.txt

dist: 
	tar -czf lab1b-40205638.tar.gz lab1b-client.c lab1b-server.c test.c Makefile README

	