CC=		gcc
CFLAGS=		-g -Wall  -Werror -std=gnu99 -Iinclude 
LD=		gcc
LDFLAGS=	-L.
AR=		ar
ARFLAGS=	rcs
TARGETS=	bin/spidey

all:		$(TARGETS)

clean:
	@echo Cleaning...
	@rm -f $(TARGETS) lib/*.a src/*.o *.log *.input

# Link Static Library

lib/libspidey.a: src/utils.o src/socket.o src/single.o src/request.o src/handler.o src/forking.o
	$(AR) $(ARFLAGS) $@ $^

# Link Executable
bin/spidey: src/spidey.o lib/libspidey.a 
	$(LD) $(LDFLAGS) -o $@ $^


# Object Files

src/spidey.o: src/spidey.c
	$(CC) $(CFLAGS) -c -o $@ $^

src/utils.o: src/utils.c
	$(CC) $(CFLAGS) -c -o $@ $^

src/socket.o: src/socket.c
	$(CC) $(CFLAGS) -c -o $@ $^

src/single.o: src/single.c
	$(CC) $(CFLAGS) -c -o $@ $^

src/request.o: src/request.c
	$(CC) $(CFLAGS) -c -o $@ $^

src/handler.o: src/handler.c
	$(CC) $(CFLAGS) -c -o $@ $^

src/forking.o: src/forking.c
	$(CC) $(CFLAGS) -c -o $@ $^


	
 
.PHONY:		all test clean

