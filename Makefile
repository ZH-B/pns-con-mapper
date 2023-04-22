CC = ./build-tool/bin/arm-none-linux-gnueabihf-g++
CFLAGS = -Wall -Werror -fPIE -fpermissive -pthread -static
LDFLAGS = \
		-static \
		-L./build-tool/arm-none-linux-gnueabihf/libc/usr/lib/ \
		-lm \
		-pthread \
		-pie 

SRCDIR = src
OBJDIR = obj
BINDIR = build

SOURCES := $(wildcard $(SRCDIR)/*.c)
OBJECTS := $(patsubst $(SRCDIR)/%.c, $(OBJDIR)/%.o, $(SOURCES))
EXECUTABLE := $(BINDIR)/con-mapper

.PHONY: all clean

all: $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(LDFLAGS) $^ -o $@

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(EXECUTABLE)
