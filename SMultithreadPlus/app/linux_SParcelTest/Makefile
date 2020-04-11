CPP := $(CROSS_COMPILE)g++
MAKE = make

DIR_TOP = ../..
DIR_LIB = $(DIR_TOP)/lib
DIR_INC = $(DIR_TOP)/inc
STATIC_LIB = $(DIR_LIB)/libSMultithreadPlus.a
CPPFLAGS = -I$(DIR_INC) -I$(DIR_TOP)/../QCommon/inc \
           -Wall -fPIC -fno-exceptions -MD $(ENV_CPPFLAGS)
EXEFLAGS = -fPIE -pie $(ENV_EXEFLAGS)

CSRCS =
CPPSRCS = ../SParcelTest.cpp

PROG = SParcelTest

OBJS = $(CSRCS:.c=.o) $(CPPSRCS:.cpp=.o)
DEPS = $(OBJS:.o=.d)

all:
	$(MAKE) -C $(DIR_TOP)/build
	$(MAKE) app -C .

app: $(PROG) $(OBJS)

%.o: %.cpp
	$(CPP) $(CPPFLAGS) -c -o $@ $<

$(PROG): $(OBJS) $(STATIC_LIB)
	$(CPP) -o $(PROG) $(OBJS) $(STATIC_LIB) $(EXEFLAGS)

clean:
	$(MAKE) -C $(DIR_TOP)/build clean
	rm -rf $(OBJS) $(PROG) $(DEPS)

-include $(DEPS)

