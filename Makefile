SOURCES = $(shell find src/ -type f)
LIBS    = -pthread -lform -lncurses
FLAGS   = -fsanitize=address

all:
	gcc -o bin/bmb_clinet ${FLAGS} ${SOURCES} ${LIBS}

.PHHONY: test

test: all