# Project Name
TARGET = StiffString

# Sources
CPP_SOURCES = main.cpp StiffString.cpp Cycle.cpp
C_SOURCES = leaflet.c

GDBFLAGS += --fullname

# Library Locations
LIBDAISY_DIR = ../../DaisyExamples/libDaisy/
DAISYSP_DIR = ../../DaisyExamples/DaisySP/
# LIBS += -lleaf
# LIBDIR += -L ../../LEAF/leaf/build
# C_INCLUDES += -I../../LEAF/leaf/Src -I../../LEAF/leaf/Inc/  -I../../LEAF/leaf/

# Core location, and generic Makefile.
SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile
