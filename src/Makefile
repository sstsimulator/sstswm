
CXX=$(shell sst-config --CXX)
CXXFLAGS=$(shell sst-config --ELEMENT_CXXFLAGS)
LDFLAGS=$(shell sst-config --ELEMENT_LDFLAGS)  -L$(SWM)/lib -lswm

CXXFLAGS+=-I$(SST_ELEMENTS)/include -I$(SWM)/include

all: libsstSwm.so install pyswm.inc

DEPS = swm.h convert.h workload.h dbg.h event.h pyswm.inc
OBJ = swm.o convert.o workload.o

%.o: %.cc $(DEPS)
	$(CXX) $(CXXFLAGS) -c -o $@ $< 

libsstSwm.so: $(OBJ)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^

install:
	sst-register sstSwm sstSwm_LIBDIR=$(CURDIR)
	sst-register SST_ELEMENT_SOURCE sstSwm=$(CURDIR)
	sst-register SST_ELEMENT_TESTS  sstSwm=$(CURDIR)/../tests

clean:
	rm -f *.o libsstSwm.so pyswm.inc

%.inc: %.py
	od -v -t x1 < $< | sed -e 's/^[^ ]*[ ]*//g' -e '/^\s*$$/d' -e 's/\([0-9a-f]*\)[ $$]*/0x\1,/g' > $@
