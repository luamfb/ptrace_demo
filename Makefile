OBJ := peek_poke.o breakpoint.o

all : peek_poke breakpoint

peek_poke : peek_poke.o

breakpoint : breakpoint.o

$(OBJ) : %.o : %.c

clean :
	rm -f $(OBJ)

.PHONY: all clean
