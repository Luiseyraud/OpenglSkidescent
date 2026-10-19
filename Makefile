# Makefile du projet « la descente du skieur »
#   make        compile le programme
#   make run    compile puis lance le programme
#   make clean  supprime les fichiers compilés

CXX      := g++
CXXFLAGS := -std=c++17 -O2 -I stb
LDLIBS   := -lglfw -lGLEW -lGL

# Tous les .cpp de src/ sont compilés en .o dans build/
SRC := $(wildcard src/*.cpp)
OBJ := $(SRC:src/%.cpp=build/%.o)
BIN := skieur

all: $(BIN)

# Édition de liens : les .o + les bibliothèques donnent l'exécutable
$(BIN): $(OBJ)
	$(CXX) $(OBJ) -o $@ $(LDLIBS)

# Compilation d'un .cpp en .o
# -MMD -MP écrit aussi un fichier .d qui liste les .h inclus :
# ainsi, modifier un .h recompile les .cpp qui l'utilisent.
build/%.o: src/%.cpp | build
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

build:
	mkdir -p build

# Lancé depuis la racine, car le programme charge shaders/, models/ et textures/
# avec des chemins relatifs.
run: $(BIN)
	./$(BIN)

clean:
	rm -rf build $(BIN)

-include $(OBJ:.o=.d)

.PHONY: all run clean
