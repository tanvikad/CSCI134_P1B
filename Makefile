output: lab1b_client.c lab1b_server.c
	gcc -Wall -Wextra lab1b_client.c -o lab1b_client
	gcc -Wall -Wextra lab1b_server.c -o lab1b_server

# debug: lab1b.c
# 	gcc -Wall -Wextra -g lab1b.c -o lab1b

client:  lab1b_client.c
	gcc -Wall -Wextra lab1b_client.c -o lab1b_client

server: lab1b_server.c
	gcc -Wall -Wextra lab1b_server.c -o lab1b_server

clean:
	rm -f *.o
	rm -f lab1b_client
	rm -f lab1b_server
	rm -f *.gz
	rm -f *.txt

dist: 
	tar -czf lab1b-40205638.tar.gz lab1b_client.c lab1b_server.c Makefile README

	