# Compiler and flags
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O3 -Inlohmann

# Target executable name
TARGET = run

# Directories
BUILD_DIR = build

# Automatically find all .cpp files in the current directory
SRCS = $(wildcard *.cpp)
# Map source files to object files inside the build directory
OBJS = $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(SRCS))

# Default rule
all: $(BUILD_DIR) $(TARGET)

# Create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Link object files to create the executable
$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

# Compile source files into object files inside the build directory
$(BUILD_DIR)/%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean up build artifacts and build directory
clean:
	rm -rf $(TARGET) $(BUILD_DIR)

# Phony targets
.PHONY: all clean