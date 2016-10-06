
CC = gcc
CFLAGS = -O2 -DRETSIGTYPE=void -DHAVE_MEMMOVE 


OBJS = main.o mime.o handlers.o log.o  
all:$(OBJS)
	$(CC) -o dhttpd $(OBJS)
clean:
	rm -f $(OBJS)
	rm -f dhttpd