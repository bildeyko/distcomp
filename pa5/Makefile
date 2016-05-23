CC = clang
CFLAGS = -std=c99 -Wall --pedantic
LFLAGS = -Llib32 -lruntime
TARGET = pa5

all: $(TARGET)

$(TARGET): main.o ipc.o queue.o
	$(CC) $(LFLAGS) main.o queue.o ipc.o -o $(TARGET)

main.o: main.c
	$(CC) -c $(CFLAGS) main.c

queue.o: queue.c
	$(CC) -c $(CFLAGS) queue.c

ipc.o: ipc.c
	$(CC) -c $(CFLAGS) ipc.c

clean:
	rm -rf *.o $(TARGET) *.log