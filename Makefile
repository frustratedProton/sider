CXX      := g++
CXXFLAGS := -std=c++23 -Wall -Wextra -Wpedantic

LDFLAGS := -lpthread

SRC_DIR   := src
BUILD_DIR := build

TARGET := $(BUILD_DIR)/redis-clone

SRCS := $(wildcard $(SRC_DIR)/*.cpp)
OBJS := $(patsubst $(SRC_DIR)/%.cpp, $(BUILD_DIR)/%.o, $(SRCS))

ifeq ($(MODE), debug)
    CXXFLAGS += -g3 -O0 -DDEBUG
else ifeq ($(MODE), asan)
    CXXFLAGS += -g3 -O0 -fsanitize=address,undefined -DDEBUG
else
    CXXFLAGS += -O2 -DNDEBUG
endif

.PHONY: all debug asan clean re

all: $(TARGET)

debug:
	$(MAKE) MODE=debug

asan:
	$(MAKE) MODE=asan
$(TARGET): $(OBJS) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $^ -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR)

re: clean all