output: lab1a.c
	gcc -Wall -Wextra lab1a.c -o lab1a

debug: lab1a.c
	gcc -Wall -Wextra -g lab1a.c -o lab1a

clean:
	rm -f *.o
	rm -f lab1a
	rm -f *.gz
	rm -f *.txt

dist: 
	tar -czf lab1a-40205638.tar.gz lab1a.c Makefile README

# check: 
	