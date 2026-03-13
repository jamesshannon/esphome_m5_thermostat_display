CXX ?= g++
CXXFLAGS += -std=c++17 -Wall -Wextra -I. -I..

TEST_SOURCES := test_arc_math.cpp test_color_logic.cpp \
	test_unit_conversion.cpp test_runtime_logic.cpp \
	test_mode_label.cpp
TEST_NAMES := $(TEST_SOURCES:.cpp=)
UI_SOURCE := ../components/m5dial_thermostat/thermostat_ui.cpp
RUNTIME_SOURCE := ../components/m5dial_thermostat/runtime_logic.cpp
BUILD_DIR := .build
TEST_BINS := $(addprefix $(BUILD_DIR)/,$(TEST_NAMES))

.PHONY: all test clean

all: $(TEST_BINS)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/test_runtime_logic: test_runtime_logic.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -DESPHOME_STUBS $< \
		$(UI_SOURCE) $(RUNTIME_SOURCE) -o $@

$(BUILD_DIR)/%: %.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -DESPHOME_STUBS $< $(UI_SOURCE) -o $@

test: $(TEST_BINS)
	@for t in $(TEST_BINS); do ./$$t; done

clean:
	rm -rf $(BUILD_DIR)
