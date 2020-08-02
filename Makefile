#*****************************************************************************
# @author Joshua Malburg (joma0364)
# joshua.malburg@colorado.edu
# Advanced Embedded Software Development
# ECEN5013-002 - Rick Heidebrecht
# @date March 7, 2018
#*****************************************************************************
#
# @file Makefile
# @brief Make targets and recipes to generate object code, executable, etc
#
#*****************************************************************************

# General / default variables for all platforms / architectures
CFLAGS = -Wall -g -O0 -Werror -pthread
CPPFLAGS = -MD -MP
TARGET = project
PLATFORM = UBUNTU
LDFLAGS = -lopencv_core -lopencv_flann -lopencv_video -lpthread -lrt
include mk_files/$(TARGET).mk
INCLDS = -I./include

ifeq ($(PLATFORM),BBG)
CROSS_COMP_NAME = arm-buildroot-linux-uclibcgnueabihf
CC = $(CROSS_COMP_NAME)-gcc
LD = $(CROSS_COMP_NAME)-ld
AR = $(CROSS_COMP_NAME)-ar
SZ = $(CROSS_COMP_NAME)-size
READELF = $(CROSS_COMP_NAME)-readelf
else 
CC = g++
LD = ld
AR = ar
SZ = size
READELF = readelf
endif

# for recursive clean
GARBAGE_TYPES := *.o *.elf *.map *.i *.asm *.d *.out *.jpg *.avi *.ppm *.pgm
DIR_TO_CLEAN = src test
DIR_TO_CLEAN += $(shell find -not -path "./.git**" -type d)
GARBAGE_TYPED_FOLDERS := $(foreach DIR,$(DIR_TO_CLEAN), $(addprefix $(DIR)/,$(GARBAGE_TYPES)))

# 
OBJS  = $(SRCS:.c=.o)
DEPS = $(OBJS:.o=.d)

.PHONY: clean
clean: 
	@$(RM) -rf $(GARBAGE_TYPED_FOLDERS) $(TARGET)
	@ echo "Clean complete"

$(OBJS): %.o : %.c %.d
	@$(CC) -c $(CPPFLAGS) $(INCLDS) $(CFLAGS) $< -o $@ 
	@ echo "Compiling $@"	

%.i : %.c
	@$(CC) -E $(CPPFLAGS) $(INCLDS) $< -o $@ 
	@ echo "Compiling $@"
	
%.d : %.c
	@$(CC) -E $(CPPFLAGS) $(INCLDS) $< -o $@ 
	@ echo "Compiling $@"
	
%.asm : %.c
	@$(CC) -S $(CPPFLAGS) $(INCLDS) $(CFLAGS) $< -o $@ 
	@ echo "Compiling $@"
	
.PHONY: build
build: $(TARGET)

.PHONY: run
run: build
ifeq ($(PLATFORM),BBG)
	scp $(TARGET) root@10.0.0.87:/usr/bin/$(TARGET)
	ssh -t root@10.0.0.87 "cd /usr/bin/ && gdbserver localhost:6666 project"
endif

.PHONY: all
all: run
	
$(TARGET): $(OBJS)
	@echo PLATFORM = $(PLATFORM)
	@$(CC) $(CPPFLAGS) $(CFLAGS) -o $(TARGET) $^ `pkg-config --libs opencv` $(LDFLAGS)
	@echo build complete
	@$(SZ) -Bx $(TARGET)
	@echo
	@$(READELF) -h $(TARGET)
