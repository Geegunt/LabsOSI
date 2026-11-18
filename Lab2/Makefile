CC = cc
CPPFLAGS = -D_POSIX_C_SOURCE=200809L
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -Werror -O2

.PHONY: all clean test pdf

all: parent child

parent: parent.c ipc.c ipc.h
	$(CC) $(CPPFLAGS) $(CFLAGS) parent.c ipc.c -o $@

child: child.c ipc.c ipc.h
	$(CC) $(CPPFLAGS) $(CFLAGS) child.c ipc.c -o $@

test: all
	python3 tests/test_lab.py

pdf:
	mkdir -p output/pdf
	$(LATEX) -o output/pdf docs/lab2.tex

LATEX = tectonic

clean:
	rm -f parent child
