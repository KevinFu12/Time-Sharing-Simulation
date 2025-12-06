CC = gcc
CFLAGS = -Wall -Wextra -O2 -std=gnu11
LDFLAGS = -lm
TARGET = timeshare

# Object files
OBJS = main.o scheduler.o task.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) $(LDFLAGS)

main.o: main.c scheduler.h
	$(CC) $(CFLAGS) -c main.c

scheduler.o: scheduler.c scheduler.h
	$(CC) $(CFLAGS) -c scheduler.c

task.o: task.c scheduler.h
	$(CC) $(CFLAGS) -c task.c

clean:
	rm -f $(TARGET) $(OBJS)

.PHONY: all clean