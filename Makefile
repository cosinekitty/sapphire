# If RACK_DIR is not defined when calling the Makefile, default to two directories above
RACK_DIR ?= ../..

# FLAGS will be passed to both the C and C++ compiler
FLAGS += -Wall
ifeq ($(SAPPHIRE_ENABLE_WERROR), 1)
	FLAGS += -Werror
endif
CFLAGS +=
CXXFLAGS +=

# Careful about linking to shared libraries, since you can't assume much about the user's environment and library search path.
# Static libraries are fine, but they should be added to this plugin's build system.
LDFLAGS +=

# Add .cpp files to the build
SOURCES += $(wildcard src/*.cpp)

# Add files to the ZIP package when running `make dist`
# The compiled plugin and "plugin.json" are automatically added.
DISTRIBUTABLES += res
DISTRIBUTABLES += $(wildcard LICENSE*)
DISTRIBUTABLES += $(wildcard presets)

# Upgrade to C++17
CXXFLAGS += -std=c++17

# Enable extra compiler warnings.
FLAGS += -Wextra -Wnull-dereference

# Include the Rack plugin Makefile framework
include $(RACK_DIR)/plugin.mk

# Remove C++11
CXXFLAGS := $(filter-out -std=c++11,$(CXXFLAGS))
