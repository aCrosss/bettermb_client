TARGET = bmb_clinet

CC = gcc

SRCDIR   = src
BUILDDIR = build
BINDIR   = bin

SOURCES = $(wildcard $(SRCDIR)/*.c)
OBJECTS = $(patsubst $(SRCDIR)/%.c, $(BUILDDIR)/%.o, $(SOURCES))

LDFLAGS      = -pthread -lform -lncurses
CFLAGS_DEBUG = -fsanitize=address


all: release

release: $(BINDIR)/$(TARGET)

debug: CFLAGS = $(CFLAGS_DEBUG)
debug: $(BINDIR)/$(TARGET)_debug


$(BINDIR)/$(TARGET): $(OBJECTS)
	$(CC) $^ -o $@ $(LDFLAGS)
	@echo "release built: $@"

$(BINDIR)/$(TARGET)_debug: $(OBJECTS)
	$(CC) $^ -o $@ $(LDFLAGS)
	@echo "debug built: $@"

$(BUILDDIR)/%.o: $(SRCDIR)/%.c | $(BUILDDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

clean:
	rm -rf $(BUILDDIR) $(BINDIR)/$(TARGET) $(BINDIR)/$(TARGET)_debug

.PHONY: all clean release debug

