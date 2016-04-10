CC = clang
CFLAGS = -std=c99 -Wall --pedantic
LFLAGS = 
TARGET = pa1

all: $(TARGET)

$(TARGET): main.o ipc.o
	$(CC) $(LFLAGS) main.o ipc.o -o $(TARGET)

main.o: main.c
	$(CC) -c $(CFLAGS) main.c

ipc.o: ipc.c
	$(CC) -c $(CFLAGS) ipc.c

clean:
	rm -rf *.o $(TARGET) *.log