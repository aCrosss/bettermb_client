SOURCES = $(shell find src/ -type f)
LIBS    = -pthread -lform -lncurses
FLAGS   = -fsanitize=address

all:
	gcc -o bin/bmb_clinet ${FLAGS} ${SOURCES} ${LIBS}

unsane:
	gcc -o bin/bmb_clinet ${SOURCES} ${LIBS}

.PHHONY: test not-sanitized

test: all
not-sanitized: unsane
