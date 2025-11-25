# ====== Variables ======
CC = gcc
CFLAGS = -Wall -g
SOURCES = main.c simulation.c interaction.c
OBJECTS = $(SOURCES:.c=.o)
TARGET = ../life_game.exe

# Version STAT
TARGET_STAT = ../life_game_stat.exe
CFLAGS_STAT = $(CFLAGS) -DSTAT

# ====== Règle par défaut ======
all: $(TARGET)

# ====== Lien de l'exécutable normal ======
$(TARGET): $(OBJECTS)
	$(CC) -o $@ $^
	rm -f $(OBJECTS)

# ====== Lien de l'exécutable STAT ======
stat: CFLAGS := $(CFLAGS_STAT)
stat: $(TARGET_STAT)

$(TARGET_STAT): $(OBJECTS)
	$(CC) -o $@ $^
	rm -f $(OBJECTS)

# ====== Compilation des fichiers .c ======
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# ====== Nettoyage ======
.PHONY: all clean fclean re stat

clean:
	rm -f $(OBJECTS)

fclean: clean
	rm -f $(TARGET) $(TARGET_STAT)

re: fclean all

# links of tutorials
# https://dev.to/ashcript/comprendre-le-makefile-exemple-avec-le-langage-c-47n9


