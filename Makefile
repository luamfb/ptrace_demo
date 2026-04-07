BUILD_DIR := build

OBJ := peek_poke.o breakpoint.o
OBJ_PATH := $(addprefix $(BUILD_DIR)/, $(OBJ))

BIN := peek_poke breakpoint
BIN_PATH := $(addprefix $(BUILD_DIR)/, $(BIN))

all : $(BIN_PATH)

$(BIN_PATH) : % : %.o | $(BUILD_DIR)

$(OBJ_PATH) : $(BUILD_DIR)/%.o : %.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c -o $@ $^

$(BUILD_DIR) :
	mkdir -p $(BUILD_DIR)

clean :
	rm -rf $(BUILD_DIR)

.PHONY: all clean
