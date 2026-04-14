# Desktop simulator for the WatchyMultiTZ watchface.
#
# Compiles every .cpp in this directory plus every .cpp under
# ../sketches/WatchyMultiTZ/src/face/ (recursively) into a single
# `multitzsim` binary that renders a frame to a PNG file.
#
# Targets:
#   make           build multitzsim
#   make run       build + run with default args (out/screenshot.png)
#   make clean     delete binary + object files

# --- Toolchain --------------------------------------------------------------
CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wno-unused-parameter

# --- Paths ------------------------------------------------------------------
SIM_DIR   := .
FACE_DIR  := ../sketches/WatchyMultiTZ/src/face
SKETCH_SRC:= ../sketches/WatchyMultiTZ/src
HAL_DIR   := ../sketches/WatchyMultiTZ/src/hal

INCLUDES  := -I$(HAL_DIR) -I$(SKETCH_SRC) -I$(SIM_DIR) -I$(SIM_DIR)/shims

# --- Sources ----------------------------------------------------------------
# Sim sources: every .cpp directly under sim/ (wildcard tolerates missing
# files, so main.cpp alone is enough to build the scaffold).
SIM_SRCS  := $(wildcard $(SIM_DIR)/*.cpp)

# Face sources: recursively collected. `shell find` returns empty if the
# directory has no .cpp yet, which is fine.
FACE_SRCS   := $(shell find $(FACE_DIR) -name '*.cpp' 2>/dev/null)

# Asset sources (flag + icon procedural drawers).
ASSET_SRCS  := $(wildcard $(SKETCH_SRC)/assets/*.cpp)

SRCS      := $(SIM_SRCS) $(FACE_SRCS) $(ASSET_SRCS)

# Object files live alongside their source (compile-per-source rule below).
OBJS      := $(SRCS:.cpp=.o)

BIN       := multitzsim

# --- Rules ------------------------------------------------------------------
.PHONY: all run clean

all: $(BIN)

$(BIN): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $@

# Pattern rule: one object per source, preserves subdirectory layout.
%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

run: $(BIN)
	./$(BIN) --out out/screenshot.png

clean:
	rm -f $(BIN) $(OBJS)
