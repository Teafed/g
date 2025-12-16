SRC_DIR = src
LIREN_DIR = liren/src
OBJ_DIR = obj
BIN_DIR = bin

# main sources
SRCS = $(shell find $(SRC_DIR) -name '*.c')
OBJS = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRCS))

# liren sources
LIREN_SRCS = $(shell find $(LIREN_DIR) -name '*.c')
LIREN_OBJS = $(patsubst $(LIREN_DIR)/%.c, $(OBJ_DIR)/LIREN/%.o, $(LIREN_SRCS))
LIREN_LIB = $(BIN_DIR)/libliren.a

# compiler flags
CC = gcc
CFLAGS = -Wall -Wextra -g -std=c99 \
	-I./$(SRC_DIR)/include \
	-I./liren/include
LDFLAGS = -lSDL2 -ldl

TARGET = $(BIN_DIR)/game

.PHONY: all clean run directories

all: directories $(TARGET)

directories:
	mkdir -p $(OBJ_DIR) $(BIN_DIR)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/liren/%.o: $(LIREN_DIR)/%.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(LIREN_LIB): $(LIREN_OBJS)
	ar rcs $@ $^

$(TARGET): $(OBJS) $(LIREN_LIB)
	$(CC) $(OBJS) $(LIREN_LIB) -o $@ $(LDFLAGS)

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)

run: all
	./$(TARGET)

debug: CFLAGS += -DDEBUG -O0
debug: all

release: CFLAGS += -O2 -DNDEBUG
release: clean all

show-files:
	@echo "Source files:"
	@echo $(SRCS)
	@echo "Liren source files:"
	@echo $(LIREN_SRCS)
	@echo "Object files:"
	@echo $(OBJS)
	@echo "Liren object files:"
	@echo $(LIREN_OBJS)
