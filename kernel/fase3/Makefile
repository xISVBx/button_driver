KDIR ?= /lib/modules/$(shell uname -r)/build
SOURCE_DIR := src
BUILD_DIR := build
MODULE_INPUTS := $(addprefix $(BUILD_DIR)/,Kbuild button_driver.c)

.PHONY: all clean

all: $(MODULE_INPUTS)
	$(MAKE) -C "$(KDIR)" M="$(CURDIR)/$(BUILD_DIR)" modules

$(MODULE_INPUTS): $(BUILD_DIR)/%: $(SOURCE_DIR)/% | $(BUILD_DIR)
	cp "$<" "$@"

$(BUILD_DIR):
	mkdir -p "$@"

clean:
	if test -d "$(BUILD_DIR)"; then find "$(BUILD_DIR)" -mindepth 1 -maxdepth 1 ! -name .gitkeep -exec rm -rf -- {} +; fi
