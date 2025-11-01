CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2 -g
INCLUDES = -Iinclude -Ilib/Fusion $(shell pkg-config --cflags dbus-1)
LIBS = -lbluetooth -lpthread -lm -lyaml -ldbus-1

SRCDIR = src
OBJDIR = obj
FUSIONDIR = lib/Fusion
SOURCES = $(wildcard $(SRCDIR)/*.c) $(wildcard $(FUSIONDIR)/*.c)
OBJECTS = $(SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/%.o)
OBJECTS := $(OBJECTS:$(FUSIONDIR)/%.c=$(OBJDIR)/%.o)
TARGET = m5-mouse-daemon

.PHONY: all clean install

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $@ $(LIBS)

$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(OBJDIR)/%.o: $(FUSIONDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(OBJDIR):
	mkdir -p $(OBJDIR)

clean:
	rm -rf $(OBJDIR) $(TARGET)

install: $(TARGET)
	sudo cp $(TARGET) /usr/local/bin/
	sudo cp ../config/m5-mouse.yaml /etc/
	sudo cp ../config/m5-mouse.service /etc/systemd/system/
	sudo systemctl daemon-reload

uninstall:
	sudo rm -f /usr/local/bin/$(TARGET)
	sudo rm -f /etc/m5-mouse.yaml
	sudo rm -f /etc/systemd/system/m5-mouse.service
	sudo systemctl daemon-reload