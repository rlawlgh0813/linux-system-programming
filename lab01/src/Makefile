# Set compiler
CC = gcc

# Target
TARGET = ssu_cleanup

# Object
OBJECTS = ssu_cleanup.o

# Compile
$(TARGET) : $(OBJECTS)
	$(CC) -o $(TARGET) $(OBJECTS)

# set
ssu_cleanup.o: ssu_cleanup.c
	$(CC) -c ssu_cleanup.c

# Remove files
clean :
	rm ssu_cleanup.o
	rm ssu_cleanup
