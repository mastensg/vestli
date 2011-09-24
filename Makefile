VERSION = 0.1

CFLAGS  = -Wall -pedantic -std=c99 -g
LDFLAGS = -lSDL -lSDL_ttf -lcurl -ljson

PREFIX  = /usr/local
BINARY  = trafikanten-table
BINPATH = $(PREFIX)/bin/

MANSECT = 1
MANPAGE = $(BINARY).$(MANSECT)
MANBASE = $(PREFIX)/share/man/man$(MANSECT)
MANPATH = $(MANBASE)/$(MANPAGE)

$(BINARY): $(BINARY).o trafikanten.o

all: $(BINARY)

clean:
	rm -f $(BINARY) *.o

install: all
	install -Ds $(BINARY) $(BINPATH)
	install -D -m 644 $(MANPAGE) $(MANPATH)
	sed -i "s/VERSION/$(VERSION)/g" $(MANPATH)

uninstall:
	rm -f $(BINPATH)
	rm -f $(MANPATH)

.PHONY: all clean install uninstall
