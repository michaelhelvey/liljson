MODE 		?= debug
CC 			= gcc
SOURCE_DIR 	= src

SRC_FILES 	= src/main.c

BUILD_DIR 	= build
EXE 		= $(BUILD_DIR)/$(MODE)/liljson
C_VERSION 	= 11

SRC_OBJS 	= $(SRC_FILES:%.c=$(BUILD_DIR)/%.o)

common_flags = -Wall -Wextra --std=c$(C_VERSION)
ifeq ($(MODE), debug)
	CFLAGS = -g
else
	CFLAGS = -Ofast
endif

CFLAGS += $(common_flags)

$(EXE): $(SRC_OBJS)
	@mkdir -p $(BUILD_DIR)/$(MODE)
	echo $(SRC_OBJS)
	$(CC) $(CFLAGS) -o $(EXE) $?

$(SRC_OBJS): $(SRC_FILES)
	@mkdir -p $(BUILD_DIR)/$(SOURCE_DIR)
	$(CC) $(CFLAGS) -c $(patsubst $(BUILD_DIR)/%.o, %.c, $*.o) -o $*.o

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)

.PHONY: run
run: $(EXE)
	@$(EXE)

.PHONY: release
release:
	@$(MAKE) MODE=release --no-print-directory

.PHONY: debug
debug:
	@$(MAKE) MODE=debug --no-print-directory
