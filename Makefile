CC		    = gcc
INCLUDES    = -I$(PWD)/include -I$(PWD)/CMore/
CFLAGS	    = $(INCLUDES) -Wno-unknown-pragmas -MMD -O0 -g -Wall -Werror -Wextra -Wformat=2 -Wshadow -pedantic -Werror=vla -march=native
LIBS		=

DEFINES	 =
DEFINES	:=

ROOT_DIR	= $(CURDIR)
INCLUDE_DIR = $(ROOT_DIR)/include
SRC_DIR     = $(ROOT_DIR)/src
OBJ_DIR     = $(ROOT_DIR)/obj
TESTS_DIR   = $(ROOT_DIR)/src/tests
RES_DIR     = $(ROOT_DIR)/res
RAW_RES_DIR = $(ROOT_DIR)/rawres
TOOLS_DIR	= $(SRC_DIR)/tools
CMORE_DIR	= $(ROOT_DIR)/CMore

DIRS_TO_MAKE   := $(OBJ_DIR) $(RES_DIR)

SRC     = $(wildcard $(SRC_DIR)/*.c)
TESTS   = $(wildcard $(TESTS_DIR)/*.c)
TOOLS   = $(wildcard $(TOOLS_DIR)/*.c)

SNEMULATOR_SRC  = $(SRC_DIR)/main.c

OBJ         = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRC))
TOOLS_OBJ   = $(patsubst $(TOOLS_DIR)/%.c,$(OBJ_DIR)/%.o,$(TOOLS))
TOOLS_BIN   = $(patsubst $(OBJ_DIR)/%.o,$(ROOT_DIR)/%,$(TOOLS_OBJ))
CMORE_STATIC_LIB = $(ROOT_DIR)/cmore.a

#DEP := $(patsubst $(OBJ_DIR)/%.o,$(OBJ_DIR)/%.d,$(OBJ))

export CMORE_STATIC_LIB
export CFLAGS
export LIBS

all: snesmulator

snesmulator: $(OBJ) $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

tests: $(OBJ) $(TESTS_OBJ) $(TEST_OBJ) $(CMORE_STATIC_LIB) | $(SHADERS_OBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

make_func_list:
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

.PHONY: tools
tools: | $(OBJ) $(CMORE_STATIC_LIB)
	$(MAKE) -C $(TOOLS_DIR) OBJ_DIR=$(OBJ_DIR)

$(CMORE_STATIC_LIB):
	$(MAKE) -e -C $(CMORE_DIR) $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(OBJ_DIR)/%.o: $(TESTS_DIR)/%.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(OBJ_DIR)/%.o: $(TOOLS_DIR)/%.c
	$(CC) -c -o $@ $< $(CFLAGS)

-include $(DEP)

.PHONY: clean
clean:
	rm -f snesmulator tests
	rm -r -f $(DIRS_TO_MAKE)
	$(MAKE) -C $(TOOLS_DIR) clean
	$(MAKE) -C $(CMORE_DIR) clean

# Make the obj directory
$(shell mkdir -p $(DIRS_TO_MAKE))
$(info $(shell \
	if [ ! -e "./CMore/.git" ];\
		then git submodule init && git submodule update;\
	fi ))
