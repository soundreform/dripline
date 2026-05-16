# Project Name
TARGET = TestBypass

# Uncomment to use DaisySP LGPL (like ReverbSc, etc.)
#USE_DAISYSP_LGPL=1

# Sources
CPP_SOURCES = TestBypass.cpp ../../src/dripline.cpp
C_INCLUDES = -I.. -I../../src

# Library Locations
LIBDAISY_DIR = ../../vendor/libDaisy
DAISYSP_DIR = ../../vendor/DaisySP

# Core location, and generic Makefile.
SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile
