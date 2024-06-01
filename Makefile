CC=gcc
CFLAGS=-Wall -Wextra -pedantic
LDFLAGS=
EXEC=ipk24chat-client
SRC=$(wildcard *.c)
OBJ=$(SRC:.c=.o)

all: $(EXEC)

$(EXEC): $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $<

clean:
	rm -rf $(OBJ) $(EXEC)

.PHONY: all clean
