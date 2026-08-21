.PHONY: all gui debug test test-sanitize clean

BUILD ?= build
TEST_BUILD ?= build-test
SANITIZE_BUILD ?= build-test-sanitize
CMAKE ?= cmake

all: gui

gui:
	$(CMAKE) -S . -B $(BUILD) -DCMAKE_BUILD_TYPE=Release -DHHMS_BUILD_GUI=ON
	$(CMAKE) --build $(BUILD) --config Release --target hhms

debug:
	$(CMAKE) -S . -B $(BUILD) -DCMAKE_BUILD_TYPE=Debug -DHHMS_BUILD_GUI=ON
	$(CMAKE) --build $(BUILD) --config Debug --target hhms

test:
	$(CMAKE) -S . -B $(TEST_BUILD) -DCMAKE_BUILD_TYPE=Release -DHHMS_BUILD_GUI=OFF
	$(CMAKE) --build $(TEST_BUILD) --config Release --target hhms_test hhms_app_test
	$(CMAKE) -E chdir $(TEST_BUILD) ctest -C Release --output-on-failure

test-sanitize:
	$(CMAKE) -S . -B $(SANITIZE_BUILD) -DCMAKE_BUILD_TYPE=Debug -DHHMS_BUILD_GUI=OFF -DHHMS_ENABLE_SANITIZERS=ON
	$(CMAKE) --build $(SANITIZE_BUILD) --config Debug --target hhms_test hhms_app_test
	$(CMAKE) -E chdir $(SANITIZE_BUILD) ctest -C Debug --output-on-failure

clean:
	$(CMAKE) -E remove_directory $(BUILD)
	$(CMAKE) -E remove_directory $(TEST_BUILD)
	$(CMAKE) -E remove_directory $(SANITIZE_BUILD)
