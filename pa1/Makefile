CC = clang
CFLAGS = -std=c99 -Wall --pedantic
LFLAGS = 
TARGET = pa1

all: $(TARGET)

$(TARGET): main.o
	$(CC) $(LFLAGS) main.o -o $(TARGET)

main.o: main.c
	$(CC) -c $(CFLAGS) main.c

clean:
	rm -rf *.o $(TARGET)