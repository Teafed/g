# directories
SRC_DIR = src
LIREN_DIR = liren/src
OBJ_DIR = obj
BIN_DIR = bin

SRCS = $(wildcard $(SRC_DIR)/*.c) $(wildcard $(SRC_DIR)/*/*.c)
LIREN_SRCS = $(wildcard $(LIREN_DIR)/*.c) $(wildcard $(LIREN_DIR)/*/*.c)
OBJS = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRCS))
LIREN_OBJS = $(patsubst $(LIREN_DIR)/%.c, $(OBJ_DIR)/liren/%.o, $(LIREN_SRCS))

# header dependency files
DEPS = $(OBJS:.o=.d) $(LIREN_OBJS:.o=.d)

LIREN_LIB = $(BIN_DIR)/libliren.a
TARGET    = $(BIN_DIR)/game

CC = gcc

CFLAGS = -g -std=c99 -Wall -Wextra -MMD -MP \
         -I./$(SRC_DIR)/include \
         -I./liren/include \
         -I./$(LIREN_DIR)

LDFLAGS = -lSDL2

.PHONY: all clean run debug release show-files

all: $(TARGET)

# compile engine sources
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# compile liren sources
$(OBJ_DIR)/liren/%.o: $(LIREN_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# liren static library
$(LIREN_LIB): $(LIREN_OBJS)
	@mkdir -p $(dir $@)
	ar rcs $@ $^

# final binary
$(TARGET): $(OBJS) $(LIREN_LIB)
	@mkdir -p $(dir $@)
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
	@echo "Engine sources:"; echo "  $(SRCS)"
	@echo "Liren sources:";  echo "  $(LIREN_SRCS)"
	@echo "Engine objects:"; echo "  $(OBJS)"
	@echo "Liren objects:";  echo "  $(LIREN_OBJS)"

# pull in auto-generated header dependencies
-include $(DEPS)