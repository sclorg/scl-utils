NAME=scl-utils
VERSION=`date +%Y%m%d`
WARNINGS?=-Wall -Wshadow -Wcast-align -Winline -Wextra -Wmissing-noreturn
CFLAGS?=-O2
CFILES=scl.c
OTHERFILES=Makefile scl_enabled macros.scl scl.1
SOURCES=$(CFILES) $(OTHERFILES)
OBJECTS=scl.o

BINDIR=/usr/bin
MANDIR=/usr/share/man
CNFDIR=/etc

all: $(NAME)

$(NAME): $(SOURCES) $(OBJECTS) $(OTHERFILES)
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJECTS) $(WARNINGS) -o scl

clean:
	rm -f *.o scl

distclean: clean
	rm -f *~

dist: $(NAME)
	LANG=C
	rm -rf $(NAME)-$(VERSION)
	mkdir $(NAME)-$(VERSION)
	cp $(SOURCES) $(NAME)-$(VERSION)
	tar fcz $(NAME)-$(VERSION).tar.gz $(NAME)-$(VERSION)
	rm -rf $(NAME)-$(VERSION)

install: $(NAME)
	mkdir -p $(DESTDIR)/$(BINDIR)
	mkdir -p $(DESTDIR)/$(CNFDIR)/rpm
	cp macros.scl $(DESTDIR)/$(CNFDIR)/rpm
	cp scl $(DESTDIR)/$(BINDIR)
	cp scl_enabled $(DESTDIR)/$(BINDIR)
	cp scl.1 $(DESTDIR)/$(MANDIR)/man1

uninstall:
	rm -f $(BINDIR)/scl $(BINDIR)/scl_enabled
	rm -f $(CNFDIR)/rpm/macros.scl
	rm -f $(MANDIR)/man1/scl.1
