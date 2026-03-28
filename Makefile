OBJ := peek_poke.o

all : peek_poke

peek_poke : peek_poke.o

$(OBJ) : %.o : %.c

clean :
	rm -f $(OBJ)

.PHONY: all clean
