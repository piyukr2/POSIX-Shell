CC = g++
CFLAGS = -Wall -Wextra -std=c++17 -g
LDFLAGS = -lreadline

# Target executable name
TARGET = shell

# Source files
SOURCES = main.cpp builtins.cpp extras.cpp parser.cpp io.cpp readline_shell.cpp


OBJECTS = $(SOURCES:.cpp=.o)


HEADERS = builtins.h extras.h parser.h io.h readline_shell.h


all: $(TARGET)


$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(TARGET) $(LDFLAGS)


%.o: %.cpp $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@


clean:
	rm -f $(OBJECTS) $(TARGET)


install-deps:
	sudo apt-get update
	sudo apt-get install libreadline-dev


rebuild: clean all

# Run the shell
run: $(TARGET)
	./$(TARGET)


debug: CFLAGS += -DDEBUG -O0
debug: $(TARGET)


release: CFLAGS += -O2 -DNDEBUG
release: $(TARGET)

.PHONY: all clean install-deps rebuild run debug release