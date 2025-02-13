# project name
PRJNAME = mod-dwarf-controller

# toolchain configuration
TOOLCHAIN_PREFIX = arm-none-eabi-

ifeq ($(CCC_ANALYZER_OUTPUT_FORMAT),)
# cpu configuration
THUMB = -mthumb
MCU = cortex-m3
endif

# build configuration
mod=$(MAKECMDGOALS)
ifeq ($(mod),$(filter $(mod),moddwarf))
CPU = LPC1778
CPU_SERIE = LPC177x_8x
endif

# target configuration
TARGET_ADDR = root@192.168.51.1

# project directories
DEVICE_INC	= ./nxp-lpc
CMSIS_INC	= ./nxp-lpc/CMSISv2p00_$(CPU_SERIE)/inc
CMSIS_SRC	= ./nxp-lpc/CMSISv2p00_$(CPU_SERIE)/src
CDL_INC 	= ./nxp-lpc/$(CPU_SERIE)Lib/inc
CDL_SRC 	= ./nxp-lpc/$(CPU_SERIE)Lib/src
APP_INC 	= ./app/inc
APP_SRC 	= ./app/src
RTOS_SRC	= ./freertos/src
RTOS_INC	= ./freertos/inc
DRIVERS_INC	= ./drivers/inc
DRIVERS_SRC	= ./drivers/src
PROTOCOL_INC = ./mod-controller-proto
OUT_DIR		= ./out

CDL_LIBS = lpc177x_8x_clkpwr.c
CDL_LIBS += lpc177x_8x_adc.c lpc177x_8x_gpio.c  lpc177x_8x_pinsel.c
CDL_LIBS += lpc177x_8x_systick.c lpc177x_8x_timer.c
CDL_LIBS += lpc177x_8x_uart.c lpc177x_8x_ssp.c
CDL_LIBS += lpc177x_8x_eeprom.c

SRC = $(wildcard $(CMSIS_SRC)/*.c) $(addprefix $(CDL_SRC)/,$(CDL_LIBS)) $(wildcard $(RTOS_SRC)/*.c) \
	  $(wildcard $(DRIVERS_SRC)/*.c) $(wildcard $(APP_SRC)/*.c)

# Object files
OBJ = $(SRC:.c=.o)
ALL_OBJ = `find -name "*.o"`

# include directories
INC = $(DEVICE_INC) $(CMSIS_INC) $(CDL_INC) $(RTOS_INC) $(DRIVERS_INC) $(APP_INC) $(USB_INC) $(PROTOCOL_INC)

# C flags
ifeq ($(CCC_ANALYZER_OUTPUT_FORMAT),)
CFLAGS += -mcpu=$(MCU)
else
CFLAGS += -DCCC_ANALYZER -Wshadow -Wno-attributes -m32
endif
CFLAGS += -std=gnu99
CFLAGS += -fcommon
CFLAGS += -Wall -Wextra -Wpointer-arith -Wredundant-decls -Wsizeof-pointer-memaccess

# CFLAGS += -Werror
CFLAGS += -Wdisabled-optimization
CFLAGS += -Wdouble-promotion -Wfloat-equal
CFLAGS += -Wformat=2 -Woverlength-strings
CFLAGS += -Wredundant-decls
CFLAGS += -Wshadow -Wundef -Wuninitialized -Wunused
CFLAGS += -Wstrict-aliasing -fstrict-aliasing
CFLAGS += -Wstrict-overflow -fstrict-overflow
CFLAGS += -Wnull-dereference
CFLAGS += -Wformat-truncation=2 -Wformat-overflow=2
CFLAGS += -Wstringop-overflow=4 -Wstringop-truncation
CFLAGS += -Wclobbered -Wlogical-op
CFLAGS += -Wduplicated-cond
CFLAGS += -Winit-self -Wwrite-strings
CFLAGS += -Wjump-misses-init -fcommon

CFLAGS += -Wa,-adhlns=$(addprefix $(OUT_DIR)/, $(notdir $(addsuffix .lst, $(basename $<))))
CFLAGS += -MMD -MP -MF $(OUT_DIR)/dep/$(@F).d
CFLAGS += -I. $(patsubst %,-I%,$(INC))
CFLAGS += -D$(CPU_SERIE)
CFLAGS += -O2

# Linker flags
LDFLAGS = -Wl,-Map=$(OUT_DIR)/$(PRJNAME).map,--cref
ifeq ($(CCC_ANALYZER_OUTPUT_FORMAT),)
LDFLAGS += -specs=rdimon.specs
LDFLAGS += -Wl,--start-group -lgcc -lc -lm -lrdimon -Wl,--end-group
LDFLAGS += -T./link/LPC.ld
else
LDFLAGS += -lm
endif

# Define programs and commands.
CC      = $(TOOLCHAIN_PREFIX)gcc
OBJCOPY = $(TOOLCHAIN_PREFIX)objcopy
OBJDUMP = $(TOOLCHAIN_PREFIX)objdump
NM      = $(TOOLCHAIN_PREFIX)nm
SIZE    = $(TOOLCHAIN_PREFIX)size

# define the output files
ELF = $(OUT_DIR)/$(PRJNAME).elf
BIN = $(OUT_DIR)/$(PRJNAME).bin
HEX = $(OUT_DIR)/$(PRJNAME).hex
SYM = $(OUT_DIR)/$(PRJNAME).sym
LSS = $(OUT_DIR)/$(PRJNAME).lss

# Colors definitions
GREEN 	= '\e[0;32m'
NOCOLOR	= '\e[0m'

ifeq ($(mod),)
all:
	@echo -e "Usage: to build, use one of the following:"
	@echo -e "\tmake moddwarf"
else
all: prebuild build
endif

moddwarf: all

ifeq ($(CCC_ANALYZER_OUTPUT_FORMAT),)
build: elf lss sym hex bin
else
build: elf
endif

# output files
elf: $(OUT_DIR)/$(PRJNAME).elf
lss: $(OUT_DIR)/$(PRJNAME).lss
sym: $(OUT_DIR)/$(PRJNAME).sym
hex: $(OUT_DIR)/$(PRJNAME).hex
bin: $(OUT_DIR)/$(PRJNAME).bin

prebuild:
ifneq ($(shell cat .last_built),$(mod))
	@make clean
endif
	@mkdir -p $(OUT_DIR)
	@mkdir -p $(OUT_DIR)/dep
	@ln -fs ./$(CPU).ld ./link/LPCmem.ld
	@ln -fs ./config-$(mod).h ./app/inc/config.h
	@echo $(mod) > .last_built

# Create final output file in ihex format from ELF output file (.hex).
$(HEX): $(ELF)
	@echo -e ${GREEN}Creating HEX${NOCOLOR}
	@$(OBJCOPY) -O ihex $< $@

# Create final output file in raw binary format from ELF output file (.bin)
$(BIN): $(ELF)
	@echo -e ${GREEN}Creating BIN${NOCOLOR}
	@$(OBJCOPY) -O binary $< $@

# Create extended listing file/disassambly from ELF output file.
# using objdump (testing: option -C)
$(LSS): $(ELF)
	@echo -e ${GREEN}Creating listing file/disassambly${NOCOLOR}
	@$(OBJDUMP) -h -S -C -r $< > $@

# Create a symbols table from ELF output file.
$(SYM): $(ELF)
	@echo -e ${GREEN}Creating symbols table${NOCOLOR}
	@$(NM) -n $< > $@

# Link: create ELF output file from object files.
$(ELF): $(OBJ)
	@echo -e ${GREEN}Linking objects: generating ELF${NOCOLOR}
	@$(CC) $(THUMB) $(CFLAGS) $(OBJ) --output $@ -nostartfiles $(LDFLAGS)

# ignore warnings for 3rd-party CPU code
freertos/src/%.o: freertos/src/%.c
	$(CC) $(CFLAGS) -o $@ -c $< -Wno-unused-parameter -Wno-implicit-fallthrough -Wno-nested-externs

%.o: %.c
	@echo -e ${GREEN}Building $<${NOCOLOR}
	@$(CC) $(THUMB) $(CFLAGS) -c $< -o $@

clean:
	@echo -e ${GREEN}Object files cleaned out $<${NOCOLOR}
	@rm -rf $(ALL_OBJ) $(OUT_DIR)

size:
	@$(SIZE) out/mod-controller.elf

install:
	scp -O $(OUT_DIR)/$(PRJNAME).bin $(TARGET_ADDR):/tmp && \
	ssh $(TARGET_ADDR) 'hmi-update /tmp/$(PRJNAME).bin && systemctl restart mod-ui'
