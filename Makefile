CC       := cc
CFLAGS   := -Wall -Wextra -Werror -std=c11
CPPFLAGS := -IStructures/include -IDaemon/include -IClient/include \
            -D_POSIX_C_SOURCE=200809L

DAEMON   := lbv-daemon
CLIENT   := lbv-client

OBJ_DIR  := build

STRUCT_SRC := $(wildcard Structures/src/*.c)
DAEMON_SRC := $(wildcard Daemon/src/*.c)
CLIENT_SRC := $(wildcard Client/src/*.c)

STRUCT_OBJ := $(STRUCT_SRC:%.c=$(OBJ_DIR)/%.o)
DAEMON_OBJ := $(DAEMON_SRC:%.c=$(OBJ_DIR)/%.o)
CLIENT_OBJ := $(CLIENT_SRC:%.c=$(OBJ_DIR)/%.o)

OBJS := $(STRUCT_OBJ) $(DAEMON_OBJ) $(CLIENT_OBJ)
DEPS := $(OBJS:.o=.d)

all: $(DAEMON) $(CLIENT)

$(DAEMON): $(DAEMON_OBJ) $(STRUCT_OBJ)
	$(CC) $^ -lsqlite3 -o $@

$(CLIENT): $(CLIENT_OBJ) $(STRUCT_OBJ)
	$(CC) $^ -lncurses -o $@

$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(CPPFLAGS) -MMD -MP -c $< -o $@

clean:
	rm -rf $(OBJ_DIR)

fclean: clean
	rm -f $(DAEMON) $(CLIENT)

re: fclean all

-include $(DEPS)

.PHONY: all clean fclean re
