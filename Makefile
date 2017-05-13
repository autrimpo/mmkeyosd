include config.mk

CC ?= gcc
LD ?= gcc
CFLAGS += -Wall
VERSION_SYM = -DVERSION=\"${VERSION}\"
XFT_INC = $(shell pkg-config --cflags xft)
XFT_LIB = $(shell pkg-config --libs xft)
OBJ = mmkeyosd.o config.o
LIBS = -lX11 $(XFT_LIB) -lXinerama

all: mmkeyosd

mmkeyosd: $(OBJ)
	@echo LD -o $@
	@$(CC) $(LDFLAGS) $(OBJ) $(LIBS) -o $@

%.o: %.c
	@echo CC $<
	@$(CC) $(CFLAGS) $(VERSION_SYM) $(XFT_INC) -c $< -o $@

${OBJ}: config.h config.mk

install: mmkeyosd
	@echo installing to $(DESTDIR)$(PREFIX)/bin...
	@mkdir -p $(DESTDIR)$(PREFIX)/bin
	@install -m755 mmkeyosd $(DESTDIR)$(PREFIX)/bin
	@mkdir -p $(DESTDIR)$(MANPREFIX)/man1
	@install -m644 mmkeyosd.1 $(DESTDIR)$(MANPREFIX)/man1/

clean:
	rm -f $(OBJ) mmkeyosd
