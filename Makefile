BUILDDIR=build

CC=gcc
FLAGS=-I$(INCLUDEDIR) -std=gnu99 -Wall -Wextra
LFLAGS=-lcjson
TARGET=server

SOURCEDIR=src
INCLUDEDIR=inc
OBJ=$(patsubst $(SOURCEDIR)/%.c, $(BUILDDIR)/%.o, $(SOURCES))

SOURCES=$(wildcard $(SOURCEDIR)/*.c)

all: $(BUILDDIR) $(BUILDDIR)/$(TARGET)

$(BUILDDIR):
	mkdir -p $@

$(BUILDDIR)/$(TARGET): $(OBJ)
	$(CC) $^ -o $@ $(LFLAGS)

$(OBJ): $(BUILDDIR)/%.o: $(SOURCEDIR)/%.c
	$(CC) -c $(FLAGS) $< -o $@

run: all
	$(BUILDDIR)/$(TARGET)

.PHONY: client

client: client/client.c
	gcc client/client.c -o $(BUILDDIR)/client
	$(BUILDDIR)/client

clean:
	rm -rf build

uml: docs
	plantuml docs/state_machine.uml

docs:
	mkdir docs
