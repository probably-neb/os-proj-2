CC 	= gcc

CFLAGS  = -Wall -g -I . -I include -I lib64

LD 	= gcc

LDFLAGS  = -Wall -g -L lib64

PROGS	= snakes nums hungry

SNAKEOBJS  = randomsnakes.o 

HUNGRYOBJS = hungrysnakes.o 

NUMOBJS    = numbersmain.o

OBJS	= $(SNAKEOBJS) $(HUNGRYOBJS) $(NUMOBJS) 

SRCS	= randomsnakes.c numbersmain.c hungrysnakes.c

HDRS	= 

EXTRACLEAN = core $(PROGS)

all: 	$(PROGS)

allclean: clean
	@rm -f $(EXTRACLEAN) *.o *.a

clean:
	rm -f $(OBJS) *~ TAGS

snakes: randomsnakes.o libLWP.a lib64/libsnakes.so
	$(CC) $(LDFLAGS) $(CFLAGS) -o snakes randomsnakes.o -L. -lncurses -lsnakes -lLWP

hungry: hungrysnakes.o libLWP.a lib64/libsnakes.so
	$(CC) $(LDFLAGS) $(CFLAGS) -o hungry hungrysnakes.o -L. -lncurses -lsnakes -lLWP

nums: numbersmain.o libLWP.a
	$(CC) $(LDFLAGS) $(CFLAGS) -o nums numbersmain.o -L. -lLWP

hungrysnakes.o: lwp.h include/snakes.h
	$(CC) $(LDFLAGS) $(CFLAGS) -fPIE -c demos/hungrysnakes.c

randomsnakes.o: lwp.h include/snakes.h
	$(CC) $(LDFLAGS) $(CFLAGS) -fPIE -c demos/randomsnakes.c

numbersmain.o: lwp.h
	$(CC) $(LDFLAGS) $(CFLAGS) -fPIE -c demos/numbersmain.c

libLWP.a: lwp.c rr.c demos/util.c
	$(CC) $(CFLAGS) -c rr.c demos/util.c lwp.c lib64/magic64.S
	ar r libLWP.a util.o lwp.o rr.o magic64.o
	rm lwp.o

submission: lwp.c rr.c util.c Makefile README
	tar -cf project2_submission.tar lwp.c rr.c Makefile README
	gzip project2_submission.tar

rs: snakes
	(export LD_LIBRARY_PATH=lib64; ./snakes)

hs: hungry
	(export LD_LIBRARY_PATH=lib64; ./hungry)

ns: nums
	(export LD_LIBRARY_PATH=lib64; ./nums)
