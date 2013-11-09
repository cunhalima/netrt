CC=gcc
#CFLAGS=-O2 -Wall -Wextra -Wno-unused-parameter -Wno-unused-variable \
#        -Wno-unused-but-set-variable -g
#CFLAGS=-O2 -Wall -Wextra -g
CFLAGS = -lm -Wall -Werror \
        -Wno-unused-but-set-variable -Wno-unused-variable -g
#LDFLAGS=-g -lm
#LDFLAGS=-g -lm
LDFLAGS=-lm -pthread
OBJS=main.o
EXE=net

.PHONY: all clean run t1 val

all: $(EXE)

clean:
	@rm -f $(OBJS)

.c.o:
	@$(CC) $(CFLAGS) -c $< -o $@

$(EXE): $(OBJS)
	@$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@

run: $(EXE)
	@./$^

t1: $(EXE)
	@./$^ < t1.txt

val: $(EXE)
	valgrind --tool=memcheck --track-origins=yes -v ./$^ < t1.txt

test: $(EXE)
	@gnome-terminal --geometry=+000+0 -e "./net 1 -e 20"
	@gnome-terminal --geometry=+480+0 -e "./net 2 -e 33"
	@gnome-terminal --geometry=+960+0 -e "./net 3 -e 20"
	@gnome-terminal --geometry=+0+300 -e "./net 4 -e 30"
	@gnome-terminal --geometry=+480+300 -e "./net 5 -e 20"
	@gnome-terminal --geometry=+960+300 -e "./net 6 -e 20"
