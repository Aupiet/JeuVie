# Variables
CC = gcc
CFLAGS = -Wall -g
SOURCES = main.c simulation.c interaction.c 
OBJECTS = $(SOURCES:.c=.o)
TARGET = ../life_game.exe

# Règle par défaut
all: $(TARGET)

# Lien de l'exécutable
# $@ -> $(TARGET)
# $^ -> $(OBJECTS)
$(TARGET): $(OBJECTS)
	$(CC) -o $@ $^
	rm -f $(OBJECTS)
	
# Compilation des fichiers .c en .o
# $< -> Premier element des pr
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Déclaration des cibles phony
.PHONY: all clean fclean re

# Nettoyage des fichiers objets
clean:
	rm -f $(OBJECTS)

# Nettoyage complet (fichiers objets et exécutable)
fclean: clean
	rm -f $(TARGET)

# Refaire la compilation
re: fclean all

# links of tutorials
# https://dev.to/ashcript/comprendre-le-makefile-exemple-avec-le-langage-c-47n9


