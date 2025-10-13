CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2 -g
INCLUDES = -Iinclude
LIBS = -lbluetooth -lpthread -lm

SRCDIR = src
OBJDIR = obj
SOURCES = $(wildcard $(SRCDIR)/*.c)
OBJECTS = $(SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/%.o)
TARGET = m5-mouse-daemon

.PHONY: all clean install

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $@ $(LIBS)

$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(OBJDIR):
	mkdir -p $(OBJDIR)

clean:
	rm -rf $(OBJDIR) $(TARGET)

install: $(TARGET)
	sudo cp $(TARGET) /usr/local/bin/
	sudo cp ../config/m5-mouse.conf /etc/
	sudo cp ../config/m5-mouse.service /etc/systemd/system/
	sudo systemctl daemon-reload

uninstall:
	sudo rm -f /usr/local/bin/$(TARGET)
	sudo rm -f /etc/m5-mouse.conf
	sudo rm -f /etc/systemd/system/m5-mouse.service
	sudo systemctl daemon-reload