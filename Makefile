# -- config --
CC								:= gcc
GAME							?= a
PROGRAM						:= $(GAME).out

SRC_DIR						:= src
BIN_DIR						:= bin
LIB_DIR						:= lib

# -- compilation flags --
WARNINGS					:= -Wall -Wextra -Wshadow -Wstrict-prototypes \
										 -Wfloat-equal -Wmissing-declarations -Wmissing-include-dirs \
										 -Wmissing-prototypes -Wredundant-decls -Wunreachable-code
CFLAGS						:= $(WARNINGS) -g -MMD -MP `pkg-config --cflags glfw3` -DCLIB_TIME_GLFW
CXXFLAGS					:= -std=c++11 -g -MMD -MP `pkg-config --cflags glfw3` -DCLIB_TIME_GLFW

# -isystem instead of -I to avoid compiler warnings on external libraries
INCFLAGS					:= -I$(SRC_DIR) \
										 $(addprefix -isystem,$(LIB_DIR)) \

LDFLAGS						:= `pkg-config --libs glfw3` -lm -framework OpenGL

SRC_FILES					:= $(shell find $(SRC_DIR) -name '*.c')
SRC_OBJ_FILES			:= $(patsubst $(SRC_DIR)/%.c,$(BIN_DIR)/%.o,$(SRC_FILES))

LIB_FILES					:= lib/glad.c
LIB_OBJ_FILES			:= $(patsubst $(LIB_DIR)/%.c,$(BIN_DIR)/%.o,$(LIB_FILES))

DEP_FILES					:= $(patsubst %.o,%.d,$(SRC_OBJ_FILES) $(LIB_OBJ_FILES))

# -- rules --
all: $(PROGRAM)

$(PROGRAM): $(SRC_OBJ_FILES) $(LIB_OBJ_FILES)
	@mkdir -p $(dir $@)
	$(CC) $^ -o $@ $(LDFLAGS)

$(BIN_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCFLAGS) $(DEFINES) -c $< -o $@

# library/external C source files
$(BIN_DIR)/%.o: $(LIB_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCFLAGS) $(DEFINES) -c $< -o $@

clean:
	rm -rf $(BIN_DIR)
	rm -f *.out

rebuild: clean all

-include $(DEP_FILES)

.PHONY: all clean rebuild
