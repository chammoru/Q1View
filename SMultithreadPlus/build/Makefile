CC := $(CROSS_COMPILE)gcc
CPP := $(CROSS_COMPILE)g++
AR := $(CROSS_COMPILE)ar
MKDIR = mkdir -p

DIR_TOP = ..
DIR_LIB = $(DIR_TOP)/lib
DIR_INC = $(DIR_TOP)/inc
DIR_SRC = $(DIR_TOP)/src

CCO_FLAGS = -Wall -fPIC -fno-short-enums -MD \
            -I$(DIR_TOP)/../QCommon/inc/ -I$(DIR_TOP) -I$(DIR_INC) -I$(DIR_SRC)
CFLAGS = $(CCO_FLAGS) $(ENV_CFLAGS)
CPPFLAGS = $(CCO_FLAGS) $(ENV_CPPFLAGS)

TARGET_LIB = libSMultithreadPlus.a

CSRCS =
CPPSRCS = $(DIR_SRC)/SMutex.cpp \
		$(DIR_SRC)/SCondition.cpp \
		$(DIR_SRC)/SThread.cpp \
		$(DIR_SRC)/SParcel.cpp \
		$(DIR_SRC)/SReqListener.cpp \
		$(DIR_SRC)/SBufferPool.cpp

OBJS = $(CSRCS:.c=.o) $(CPPSRCS:.cpp=.o)
DEPS = $(OBJS:.o=.d)

all: $(OBJS) $(TARGET_LIB)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

%.o: %.cpp
	$(CPP) $(CPPFLAGS) -c -o $@ $<

$(TARGET_LIB): $(OBJS)
	$(MKDIR) $(DIR_LIB)
	$(AR) cr $(DIR_LIB)/$(TARGET_LIB) $(OBJS)

clean:
	rm -rf $(DIR_LIB) $(OBJS) $(DEPS)

-include $(DEPS)

