output: lab1b.c
	gcc -Wall -Wextra lab1b.c -o lab1b

debug: lab1b.c
	gcc -Wall -Wextra -g lab1b.c -o lab1b

clean:
	rm -f *.o
	rm -f lab1b
	rm -f *.gz
	rm -f *.txt

dist: 
	tar -czf lab1b-40205638.tar.gz lab1b.c Makefile README

# check: 
	