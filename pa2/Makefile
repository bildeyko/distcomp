CC = clang
CFLAGS = -std=c99 -Wall --pedantic
LFLAGS = -Llib32 -lruntime
TARGET = pa2

all: $(TARGET)

$(TARGET): main.o ipc.o bank_robbery.o
	$(CC) $(LFLAGS) main.o ipc.o bank_robbery.o -o $(TARGET)

main.o: main.c
	$(CC) -c $(CFLAGS) main.c

ipc.o: ipc.c
	$(CC) -c $(CFLAGS) ipc.c

bank_robbery.o: bank_robbery.c
	$(CC) -c $(CFLAGS) bank_robbery.c

clean:
	rm -rf *.o $(TARGET) *.log