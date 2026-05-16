# Project Name
TARGET = GrainularDelay

# Uncomment to use DaisySP LGPL (like ReverbSc, etc.)
#USE_DAISYSP_LGPL=1

# Sources
CPP_SOURCES = main.cpp ../../src/dripline.cpp

C_INCLUDES = -I.. -I../../src

# Library Locations
LIBDAISY_DIR = ../../vendor/libDaisy
DAISYSP_DIR = ../../vendor/DaisySP

# Core location, and generic Makefile.
SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile

# Host-based test runner. 
# Bypasses libDaisy hardware headers to avoid STM32 device errors.
.PHONY: test
test:
	g++ -std=c++14 -DUNIT_TEST -I. -I$(DAISYSP_DIR)/Source -I../../src ./src/test.cpp -o grain_test
	./grain_test && rm grain_test
