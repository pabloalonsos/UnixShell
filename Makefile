all: shell.c
	gcc -std=c99 -Wall -o shell shell.c

clean:
	$(RM) shell
