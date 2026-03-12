CXX ?= g++
CXXFLAGS += -std=c++17 -Wall -Wextra -I. -I..

TEST_SOURCES := test_arc_math.cpp test_color_logic.cpp test_unit_conversion.cpp
UI_SOURCE := ../components/m5dial_thermostat/thermostat_ui.cpp
TEST_BINS := $(TEST_SOURCES:.cpp=)

.PHONY: all test clean

all: $(TEST_BINS)

%: %.cpp
	$(CXX) $(CXXFLAGS) -DESPHOME_STUBS $< $(UI_SOURCE) -o $@

test: $(TEST_BINS)
	@for t in $(TEST_BINS); do ./$$t; done

clean:
	rm -f $(TEST_BINS)
