FLAGS=-fsanitize=address -fsanitize=leak
CFLAGS=-Wall -Wextra -g $(FLAGS)
LDFLAGS=$(FLAGS)

main: main.o

clean:
	rm -rfv main main.o
