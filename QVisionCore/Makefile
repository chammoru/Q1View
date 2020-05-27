CC := $(CROSS_COMPILE)gcc
CPP := $(CROSS_COMPILE)g++
AR := $(CROSS_COMPILE)ar

CCO_FLAGS = -Wall -fno-short-enums -fPIC -MD \
            -I../SMultithreadPlus/inc -I../QCommon/inc -I.
CFLAGS := $(CCO_FLAGS) $(ENV_CFLAGS)
CPPFLAGS := $(CCO_FLAGS) $(ENV_CPPFLAGS)

TARGET_LIB := libQVisionCore.a

CSRCS :=
CPPSRCS := QImageOcv.cpp \
           QImageMatcher.cpp \
		   QCvUtil.cpp

OBJS := $(CSRCS:.c=.o) $(CPPSRCS:.cpp=.o)
DEPS := $(OBJS:.o=.d)

all: $(OBJS) $(TARGET_LIB)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

%.o: %.cpp
	$(CPP) $(CPPFLAGS) -c -o $@ $<

$(TARGET_LIB): $(OBJS)
	$(AR) cr $(TARGET_LIB) $(OBJS)

clean:
	rm -f $(TARGET_LIB) $(OBJS) $(DEPS)

-include $(DEPS)

