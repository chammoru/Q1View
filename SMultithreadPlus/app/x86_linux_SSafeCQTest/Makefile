###############################################################################
# System definitions
###############################################################################
CPU = ARM

###############################################################################
# Tool chain setting and exporting
###############################################################################
CC = gcc
CPP = g++
AS = as
LD = ld
AR = ar
STRIP = strip
MAKE = make
RM = rm -rf

###############################################################################
# Basic directory exporting
###############################################################################
# Top directory
DIR_TOP = ../..

# Library target directory
DIR_LIB = $(DIR_TOP)/lib

# Dynamic library target directory
DIR_DSO = $(DIR_TOP)/dso

# Include directory
DIR_INC = $(DIR_TOP)/inc

###############################################################################
# Basic directory exporting
###############################################################################
EXT_LIB = .a

###############################################################################
# CPP flags
###############################################################################
# Basic flags
CPPFLAGS_BSC = -Wall -fPIC -fno-exceptions \
			  -fno-rtti -fno-short-enums -m32

# Include
CPPFLAGS_BSC += -I$(DIR_INC)

# Flags for release
CPPFLAGS_REL = $(CPPFLAGS_BSC) -O2

# Flags for debug
CPPFLAGS_DBG = $(CPPFLAGS_BSC) -g

###############################################################################
# LD flags
###############################################################################

# Basic flags
LDFLAGS_BSC =

# Flags for release
LDFLAGS_REL = $(LDFLAGS_BSC) $(LDFLAGS_LIB)

# Flags for debug
LDFLAGS_DBG = $(LDFLAGS_BSC) $(LDFLAGS_LIB) -g

###############################################################################
# Targets
###############################################################################
TARGET = SSafeCQTest

###############################################################################
# Sources
###############################################################################
CPPSRCS = ../SSafeCQTest.cpp

###############################################################################
# Objects
###############################################################################
OBJS = $(CPPSRCS:.cpp=.o)

###############################################################################
# Build Commands
###############################################################################

SOFLAGS = -Bdynamic \
	-Wl,--gc-sections \
	-Wl,-z,nocopyreloc \
	-Wl,--no-undefined

ELFFLAGS =

LIBFLAGS = -m32 -lc -lpthread -lrt -lstdc++

all: rel

# For release
rel:
	$(MAKE) -C $(DIR_TOP)/build/x86_linux rel
	$(MAKE) CPPFLAGS="$(CPPFLAGS_REL)" src -C .
	$(CPP) -o $(TARGET) $(OBJS) -L$(DIR_LIB) -lSMultithreadPlus $(SOFLAGS) $(ELFFLAGS) $(LIBFLAGS) $(LDFLAGS_REL)
	$(STRIP) $(TARGET)

# For debug
dbg:
	$(MAKE) -C $(DIR_TOP)/build/x86_linux dbg
	$(MAKE) CPPFLAGS="$(CPPFLAGS_DBG)" src -C .
	$(CPP) -o $(TARGET) $(OBJS) -L$(DIR_LIB) -lSMultithreadPlus $(SOFLAGS) $(ELFFLAGS) $(LIBFLAGS) $(LDFLAGS_DBG)

src: $(OBJS)

clean:
	$(MAKE) -C $(DIR_TOP)/build/x86_linux clean
	$(RM) $(OBJS) $(TARGET)

%.o: %.cpp
	$(CPP) -c -o $@ $(CPPFLAGS) $<

