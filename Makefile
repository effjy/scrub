# scrub — forensic-grade metadata scrubber

CC      ?= cc
CFLAGS  ?= -std=c11 -O2 -Wall -Wextra -Wpedantic -D_DEFAULT_SOURCE
PREFIX  ?= /usr/local
BINDIR   = $(PREFIX)/bin

SRC = src/scrub.c src/jpeg.c src/png.c
BIN = scrub

.PHONY: all clean install uninstall

all: $(BIN)

$(BIN): $(SRC) src/scrub.h
	$(CC) $(CFLAGS) -o $@ $(SRC)

install: $(BIN)
	install -d $(DESTDIR)$(BINDIR)
	install -m 0755 $(BIN) $(DESTDIR)$(BINDIR)/$(BIN)

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(BIN)

clean:
	rm -f $(BIN)
