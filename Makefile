NAME		= webserv

CC			= c++
FLAGS		= -Wall -Wextra -Werror -std=c++98 -I.
RM			= rm -rf

OBJDIR		= .objFiles
FILES		= webserv
SRC			= $(FILES:=.cpp)
OBJ			= $(addprefix $(OBJDIR)/, $(FILES:=.o))
HEADER		= includes/webserv.hpp

#Colors:
GREEN		=	\033[92;5;118m
YELLOW		=	\033[93;5;226m
GRAY		=	\033[33;2;37m
RESET		=	\033[0m
CURSIVE		=	\033[33;3m

.PHONY: all clean fclean re

all: $(NAME)

$(NAME): $(OBJ) $(HEADER)
	@printf "$(CURSIVE)$(GRAY) 	- Compiling $(NAME)... $(RESET)\n"
	@$(CC) $(OBJ) $(INCLUDES) -o $(NAME)
	@printf "$(GREEN)- Executable ready.\n$(RESET)"


$(OBJDIR)/%.o: %.cpp $(HEADER)
	@mkdir -p $(OBJDIR)
	@$(CC) $(FLAGS) -c $< -o $@

clean:
	@$(RM) $(OBJDIR)
	@printf "$(YELLOW)    - Object files removed.$(RESET)\n"

fclean: clean
	@$(RM) $(NAME)
	@printf "$(YELLOW)    - Executable removed.$(RESET)\n"

re: fclean all
