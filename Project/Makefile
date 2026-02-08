# ==============================================================================
#   PTL PROJECT MAKEFILE
#   Target: Cortex-M3 (MPS2-AN385) on QEMU
# ==============================================================================

# ------------------------------------------------------------------------------
# 1. PATHS & DIRECTORIES
# ------------------------------------------------------------------------------
FREERTOS_ROOT   := ../FreeRTOS
PROJECT_FOLDER  := .
UTILS_FOLDER    := ./Utils
PTL_FOLDER      := ./PTL
TESTS_FOLDER    := ./Tests
OUTPUT_DIR      := ./Output
TOOLS_FOLDER    := ./Tools

# FreeRTOS Kernel Paths
KERNEL_DIR      := $(FREERTOS_ROOT)
KERNEL_PORT_DIR := $(KERNEL_DIR)/portable/GCC/ARM_CM3

# ------------------------------------------------------------------------------
# 2. BUILD ARTIFACTS & TOOLCHAIN
# ------------------------------------------------------------------------------
DEMO_NAME       := Project
ELF             := $(OUTPUT_DIR)/$(DEMO_NAME).elf
MAP             := $(OUTPUT_DIR)/$(DEMO_NAME).map

# Compiler Tools
CC              := arm-none-eabi-gcc
LD              := arm-none-eabi-gcc
SIZE            := arm-none-eabi-size

# Target Architecture
MACHINE         := mps2-an385
CPU             := cortex-m3

# Debug Flags (Port 1234, Wait for connection)
QEMU_FLAGS_DBG  := -s -S

# ------------------------------------------------------------------------------
# 3. SOURCE FILES & VPATH
# ------------------------------------------------------------------------------

# Search paths for source files
VPATH += $(KERNEL_DIR) $(KERNEL_PORT_DIR) $(KERNEL_DIR)/portable/MemMang
VPATH += $(PROJECT_FOLDER)
VPATH += $(UTILS_FOLDER)
VPATH += $(PTL_FOLDER)/source
VPATH += $(TESTS_FOLDER)

# Kernel Sources
SOURCE_FILES += $(KERNEL_DIR)/list.c
SOURCE_FILES += $(KERNEL_DIR)/tasks.c
SOURCE_FILES += $(KERNEL_DIR)/queue.c
SOURCE_FILES += $(KERNEL_DIR)/portable/MemMang/heap_4.c
SOURCE_FILES += $(KERNEL_DIR)/portable/GCC/ARM_CM3/port.c

# Project Sources
SOURCE_FILES += $(UTILS_FOLDER)/uart.c
SOURCE_FILES += $(UTILS_FOLDER)/burner.c
SOURCE_FILES += ./startup.c

# PTL Sources
SOURCE_FILES += $(PTL_FOLDER)/source/ptl_trace.c
SOURCE_FILES += $(PTL_FOLDER)/source/ptl_wrapper.c
SOURCE_FILES += $(PTL_FOLDER)/source/ptl_scheduler.c

# Entry Point (Can be overridden via CLI: make ENTRY_POINT=...)
ENTRY_POINT  ?= $(PROJECT_FOLDER)/main.c
SOURCE_FILES += $(ENTRY_POINT)

# Create list of object files
OBJS = $(patsubst %.c, $(OUTPUT_DIR)/%.o, $(notdir $(SOURCE_FILES)))


# ------------------------------------------------------------------------------
# 4. COMPILER FLAGS (CFLAGS)
# ------------------------------------------------------------------------------
INCLUDE_DIRS  = -I$(KERNEL_DIR)/include -I$(KERNEL_PORT_DIR)
INCLUDE_DIRS += -I$(PROJECT_FOLDER)
INCLUDE_DIRS += -I$(UTILS_FOLDER)
INCLUDE_DIRS += -I$(PTL_FOLDER)/include

CFLAGS  = $(INCLUDE_DIRS)
CFLAGS += -ffreestanding
CFLAGS += -mcpu=$(CPU)
CFLAGS += -mthumb
CFLAGS += -Wall -Wextra -Wshadow
CFLAGS += -g3
CFLAGS += -Os
CFLAGS += -ffunction-sections -fdata-sections

# ------------------------------------------------------------------------------
# 5. LINKER FLAGS (LDFLAGS)
# ------------------------------------------------------------------------------
LDFLAGS  = -T ./mps2_m3.ld
LDFLAGS += -nostartfiles
LDFLAGS += -specs=nano.specs
LDFLAGS += -specs=nosys.specs
LDFLAGS += -Xlinker -Map=$(MAP)
LDFLAGS += -Xlinker --gc-sections
LDFLAGS += -mcpu=$(CPU) -mthumb

# ------------------------------------------------------------------------------
# 6. BUILD RULES
# ------------------------------------------------------------------------------

.PHONY: all clean cleanobj qemu_start qemu_debug gdb_start tests generate_config

all: $(ELF)

# Link
$(ELF): $(OBJS) ./mps2_m3.ld Makefile
	@echo "\n--- Final linking ---"
	$(LD) $(LDFLAGS) $(OBJS) -o $(ELF)
	@echo "--- Size Summary ---"
	@$(SIZE) $(ELF)

# Compile
$(OUTPUT_DIR)/%.o: %.c Makefile | $(OUTPUT_DIR)
	@echo " [CC] $<"
	@$(CC) $(CFLAGS) -c $< -o $@

# Create Output Dir
$(OUTPUT_DIR):
	mkdir -p $(OUTPUT_DIR)

cleanobj:
	rm -f $(OUTPUT_DIR)/*.o

clean:
	rm -rf $(ELF) $(MAP) $(OUTPUT_DIR)/*.o $(OUTPUT_DIR)

# ------------------------------------------------------------------------------
# 7. SIMULATION & DEBUGGING
# ------------------------------------------------------------------------------

qemu_start:
	qemu-system-arm -machine $(MACHINE) -cpu $(CPU) -kernel \
		$(ELF) -monitor none -nographic -serial stdio

qemu_debug:
	qemu-system-arm -machine $(MACHINE) -cpu $(CPU) -kernel \
		$(ELF) -monitor none -nographic -serial stdio $(QEMU_FLAGS_DBG)

gdb_start:
	arm-none-eabi-gdb -ex "target remote localhost:1234" $(ELF)

# ------------------------------------------------------------------------------
# 8. AUTOMATED TESTING FRAMEWORK
# ------------------------------------------------------------------------------



# EXPLICIT TEST ORDER: Safety -> Policies -> Preemption -> Edge Cases -> Stress -> Performance
TEST_ORDER  = $(TESTS_FOLDER)/test_init_safety.c
TEST_ORDER += $(TESTS_FOLDER)/test_safety_limits.c
TEST_ORDER += $(TESTS_FOLDER)/test_policy_kill_trace.c
TEST_ORDER += $(TESTS_FOLDER)/test_policy_skip_trace.c
TEST_ORDER += $(TESTS_FOLDER)/test_policy_catchup_trace.c
TEST_ORDER += $(TESTS_FOLDER)/test_preemption.c
TEST_ORDER += $(TESTS_FOLDER)/test_edge_minimal_gap.c
TEST_ORDER += $(TESTS_FOLDER)/test_mixed_trace.c
TEST_ORDER += $(TESTS_FOLDER)/test_production_check.c
TEST_ORDER += $(TESTS_FOLDER)/test_load_on_trace.c

# Duration to run each test before killing QEMU
TEST_DURATION := 5

# "run_single_test"
# 1. Compiles the specific test (ENTRY_POINT).
# 2. Runs QEMU in background.
# 3. Waits TEST_DURATION seconds.
# 4. Kills QEMU and checks log for [PASS].
run_single_test: cleanobj $(ELF)
	@echo "   -> Compiling Test: $(notdir $(ENTRY_POINT))"
	@echo "   -> Running QEMU ($(TEST_DURATION)s)..."
	@qemu-system-arm -machine $(MACHINE) -cpu $(CPU) \
		-kernel $(ELF) -monitor none -nographic -serial stdio \
		> qemu_test.log 2>&1 & \
		PID=$$!; \
		sleep $(TEST_DURATION); \
		kill $$PID > /dev/null 2>&1 || true
	@if grep -q "\[PASS\]" qemu_test.log; then \
		echo "   -> RESULT: PASSED ✅"; \
		echo "   -> Full Log:"; \
		cat qemu_test.log; \
	else \
		echo "   -> RESULT: FAILED ❌"; \
		echo "   -> DEBUG OUTPUT (First 20 lines):"; \
		head -n 20 qemu_test.log; \
		exit 1; \
	fi



# "tests" - The Master Test Runner
tests:
	@echo "\n========================================"
	@echo "   STARTING AUTOMATED REGRESSION SUITE   "
	@echo "========================================"
	@for test in $(TEST_ORDER); do \
		echo "\n----------------------------------------"; \
		echo "[TESTING] $$(basename $$test)"; \
		$(MAKE) --no-print-directory run_single_test ENTRY_POINT=$$test || exit 1; \
	done
	@echo "\n========================================"
	@echo "   ✅ ALL TESTS PASSED SUCCESSFULLY      "
	@echo "========================================"

# ------------------------------------------------------------------------------
# 9. PYTHON CONFIGURATION TOOL
# ------------------------------------------------------------------------------

generate_config:
	@echo "\n[TOOL] Launching Python Configurator..."
	@python3 $(TOOLS_FOLDER)/framework.py