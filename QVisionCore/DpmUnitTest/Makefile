CPP := $(CROSS_COMPILE)g++

DIR_LIB =
STATIC_LIB = ../libQVisionCore.a ../../SMultithreadPlus/lib/libSMultithreadPlus.a
CPPFLAGS = -Wall -fPIC -fno-short-enums -MD \
		   -I../../SMultithreadPlus/inc -I.. \
           $(ENV_CPPFLAGS)
EXEFLAGS = -fPIE -pie $(ENV_EXEFLAGS)

CSRCS =
CPPSRCS = ./Main.cpp

PROG = DpmUnitTest

OBJS = $(CSRCS:.c=.o) $(CPPSRCS:.cpp=.o)
DEPS = $(OBJS:.o=.d)

all:
	make -C ../../SMultithreadPlus/build
	make -C ..
	make here

here: $(OBJS) $(PROG)

%.o: %.cpp
	$(CPP) $(CPPFLAGS) -c -o $@ $<

$(PROG): $(OBJS) $(STATIC_LIB)
	$(CPP) -o $(PROG) $(OBJS) $(STATIC_LIB) $(EXEFLAGS)

clean:
	make -C ../../SMultithreadPlus/build clean
	make -C .. clean
	rm -f $(PROG) $(OBJS) $(DEPS)

-include $(DEPS)

