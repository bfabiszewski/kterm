CC=gcc
CFLAGS=-O2 -Wall `pkg-config --cflags --libs gtk+-2.0 glib-2.0` -ltermcap

#LIBVTE=-lvte
# static libvte
LIBVTE=lib/libvte.a

all:
	$(CC) kterm.c parse_config.c $(LIBVTE) $(CFLAGS) -o bin/kterm
clean:
	rm kterm
