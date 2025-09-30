# ---- toolchain & flags ----
CXX := g++
CXXFLAGS := -O2 -std=gnu++17 -IC:/itpp/include
LDFLAGS := -LC:/itpp/lib -litpp

# ---- files ----
SRC := $(wildcard src/*.cpp)
OBJ := $(patsubst src/%.cpp,build/%.o,$(SRC))
EXE := bin/main.exe

# ---- targets ----
all: $(EXE)

$(EXE): $(OBJ) | bin
	$(CXX) -o $@ $^ $(LDFLAGS)

# Compile each .cpp in src/ to a matching .o in build/
build/%.o: src/%.cpp | build
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Ensure output dirs exist
build:
	mkdir -p $@
bin:
	mkdir -p $@

clean:
	- rm -f build/*.o $(EXE)
