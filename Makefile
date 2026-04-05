#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------
ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment.")
endif

TOPDIR ?= $(CURDIR)

include $(DEVKITARM)/3ds_rules

#---------------------------------------------------------------------------------
TARGET  := Looper
BUILD   := build
SOURCES := source
INCLUDES := source

LIBS := -lctru

#---------------------------------------------------------------------------------
include $(DEVKITARM)/base_rules