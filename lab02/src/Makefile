# Set compiler
CC = gcc

# Target executable
TARGET = ssu_cleanupd

# Object files
OBJECTS = main.o command.o daemon.o arrange.o

# Default rule: build TARGET from OBJECTS
$(TARGET): $(OBJECTS)
	$(CC) -o $(TARGET) $(OBJECTS)

# Compile rules
main.o: main.c header.h
	$(CC) -c main.c

command.o: command.c header.h
	$(CC) -c command.c

daemon.o: daemon.c header.h
	$(CC) -c daemon.c

arrange.o: arrange.c header.h
	$(CC) -c arrange.c

# Clean up build artifacts
clean:
	rm -f $(OBJECTS) $(TARGET)

.PHONY: clean
