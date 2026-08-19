.PHONY: all debug test gui clean

BUILD ?= build
CMAKE ?= cmake

all: gui

$(BUILD)/CMakeCache.txt:
	$(CMAKE) -B $(BUILD) -DCMAKE_BUILD_TYPE=Release -DHHMS_BUILD_GUI=ON

debug:
	$(CMAKE) -B $(BUILD) -DCMAKE_BUILD_TYPE=Debug -DHHMS_BUILD_GUI=ON
	$(CMAKE) --build $(BUILD)

test: $(BUILD)/CMakeCache.txt
	$(CMAKE) --build $(BUILD) --target hhms_test
	$(CMAKE) -E chdir $(BUILD) ctest --output-on-failure

gui: $(BUILD)/CMakeCache.txt
	$(CMAKE) --build $(BUILD) --target hhms

clean:
	rm -rf $(BUILD)
