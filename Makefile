CC       ?= gcc
CFLAGS   ?= -std=gnu11 -Wpedantic -Wall -Wextra -g
LDFLAGS  ?=
PROGRAM  := command_line
TEST     := test

EXE_EXT :=
ifeq ($(OS),Windows_NT)
  EXE_EXT := .exe
endif

CORE_OBJS := jumbo_file_system.o basic_file_system.o raw_disk.o

all: $(PROGRAM)$(EXE_EXT) $(TEST)$(EXE_EXT)

%.o: %.c jumbo_file_system.h basic_file_system.h raw_disk.h
	$(CC) $(CFLAGS) -c -o $@ $<

$(PROGRAM)$(EXE_EXT): command_line.o $(CORE_OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

$(TEST)$(EXE_EXT): test.o $(CORE_OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

.PHONY: clean check

clean:
	rm -f *.o $(PROGRAM) $(PROGRAM).exe $(TEST) $(TEST).exe DISK TEST_DISK

check: $(TEST)$(EXE_EXT)
	./$(TEST)$(EXE_EXT)
