#==============================================================================#
#                                   SETTINGS                                   #
#==============================================================================#

NAME = ircserv

CC = c++
FLAGS = -Wall -Wextra -Werror -std=c++98 -Iinclude

RM = rm -rf
VAL = valgrind --leak-check=full --show-leak-kinds=all

SRC = main.cpp \
      srcs/utils/Utils.cpp \
      srcs/client/Client.cpp \
      srcs/channel/Channel.cpp \
      srcs/server/Server.cpp \
      srcs/server/ServerSocket.cpp \
      srcs/server/ServerLoop.cpp \
      srcs/command/Parser.cpp \
      srcs/command/CommandHandler.cpp \
      srcs/command/auth/Pass.cpp \
      srcs/command/auth/Nick.cpp \
      srcs/command/auth/User.cpp \
      srcs/command/channel/Join.cpp \
      srcs/command/channel/Part.cpp \
      srcs/command/channel/Topic.cpp \
      srcs/command/messaging/Privmsg.cpp \
      srcs/command/messaging/Notice.cpp \
      srcs/command/oper/Kick.cpp \
      srcs/command/oper/Invite.cpp \
      srcs/command/oper/Mode.cpp

OBJ_DIR = obj
OBJS = $(SRC:%.cpp=$(OBJ_DIR)/%.o)

#==============================================================================#
#                                    EXTRA                                     #
#==============================================================================#

ECHO = @echo
GREEN = \033[1;32m
RED = \033[1;31m
CYAN = \033[0;36m
MAGENTA = \033[0;35m
YELLOW  = \033[1;33m
RESET = \033[0m

#==============================================================================#
#                                    RULES                                     #
#==============================================================================#

all: $(NAME)

$(OBJ_DIR):
	@mkdir -p $(OBJ_DIR)

$(OBJ_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	@$(CC) $(FLAGS) -c $< -o $@
	@echo "$(YELLOW)[Compiled]$(RESET) $<"

$(NAME): $(OBJS)
	@$(CC) $(FLAGS) $(OBJS) -o $(NAME)
	@echo "╔══════════════════════════╗"
	@echo "║ ✅ Compiled Successfully!║"
	@echo "╚══════════════════════════╝"

r: all
	@./$(NAME)

rv: all
	@$(VAL) ./$(NAME)

clean:
	@$(RM) $(OBJS)
	@echo "$(RED)[CLEAN] Object files removed.$(RESET)"

fclean: clean
	@$(RM) $(NAME) $(OBJ_DIR)
	@echo "$(RED)[FCLEAN] Binary removed.$(RESET)"

re: fclean all

# Phony Targets
.PHONY: all clean fclean re r rv