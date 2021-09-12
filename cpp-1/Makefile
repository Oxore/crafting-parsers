COMPILE_FLAGS=-fsanitize=address -g -Wall -Wextra -Wpedantic
CFLAGS=$(COMPILE_FLAGS)
CXXFLAGS=$(COMPILE_FLAGS)
LDFLAGS=-fsanitize=address

all: main ascii

clean:
	rm -rfv main main.o ascii ascii.o
