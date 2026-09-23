BUILD_DIR := build
CC = clang
LD = ld.lld

MIPS_FLAGS := -target mips -mips2 -mabi=32 -O2 -G0 -mno-abicalls -mno-odd-spreg \
              -fomit-frame-pointer -ffunction-sections -nostdinc -I include
LDFLAGS := -nostdlib -T mod.ld --unresolved-symbols=ignore-all --emit-relocs -e 0 --no-nmagic -gc-sections

.PHONY: all
all: $(BUILD_DIR)/mod.elf

$(BUILD_DIR):
ifeq ($(OS),Windows_NT)
	@if not exist "$(BUILD_DIR)" mkdir "$(BUILD_DIR)"
else
	mkdir -p $(BUILD_DIR)
endif

$(BUILD_DIR)/mod.o: src/mod.c include/modding.h include/ra_ui.h | $(BUILD_DIR)
	$(CC) $(MIPS_FLAGS) -c $< -o $@

$(BUILD_DIR)/ui.o: src/ui.c include/ra_ui.h include/ra_ui_api.h include/modding.h | $(BUILD_DIR)
	$(CC) $(MIPS_FLAGS) -c $< -o $@

$(BUILD_DIR)/mod.elf: $(BUILD_DIR)/mod.o $(BUILD_DIR)/ui.o mod.ld
	$(LD) $(BUILD_DIR)/mod.o $(BUILD_DIR)/ui.o $(LDFLAGS) -o $@
