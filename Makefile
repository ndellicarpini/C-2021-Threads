CFLAGS = -Wall -Werror

spectacular: main.c
	gcc -pthread -o spectacular main.c $(CFLAGS)

clean:
	rm -f spectacular

all: spectacular