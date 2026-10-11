TARGET = audio_fx
OPT = -O2
CPP_STANDARD = -std=gnu++17

CPP_SOURCES = \
    main.cpp \
    ../../touch/knobs.cpp \
    ../../touch/pads.cpp \
    ../../touch/switches.cpp

C_INCLUDES = \
    -I. \
    -I../../ \
    -I../../touch

LIBDAISY_DIR ?= ../../lib/libDaisy
DAISYSP_DIR ?= ../../lib/DaisySP

SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile
