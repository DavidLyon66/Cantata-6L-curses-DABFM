CC=g++
CFLAGS=-ggdb -Wall -I/usr/include/ncursesw
LDFLAGS=-L/usr/lib
LIBRARIES=-lkeystonecomm -lncurses
SRC=cantataCDAB.cpp
OBJECTS=cantataCDAB.o
EXEC=cantataCDAB

$(EXEC) : $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) -o $(EXEC) $(LDFLAGS) $(LIBRARIES)

$(OBJECTS) : $(SRC)
	$(CC) $(CFLAGS) -c $(SRC)

clean:
	rm -rf *.o $(EXEC)
