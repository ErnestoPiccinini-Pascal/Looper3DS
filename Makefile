#---------------------------------------------------------------------------------
# Variabili progetto
#---------------------------------------------------------------------------------
TARGET   := LoopingStation        # nome finale del file .3dsx
BUILD    := build         # cartella output
SOURCES  := main.cpp       # dove ci sono i .cpp
INCLUDES := source        # dove ci sono gli header
LIBS     := -lctru        # libreria base per 3DS (NDSP, HID, GFX)

#---------------------------------------------------------------------------------
# Include sistema devkitPro
#---------------------------------------------------------------------------------
ifeq ($(strip $(DEVKITPRO)),)
$(error "DEVKITPRO non trovato. Apri il terminale MSYS2 DevkitPro.")
endif


include $(DEVKITPRO)/devkitARM/3ds_rules
include $(DEVKITPRO)/devkitARM/base_rules