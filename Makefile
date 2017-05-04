# Compile the TD analysis.
# Bryan Moffit, August 2003

DEBUG=1
QUIET	?= 1
#
ifeq ($(QUIET),1)
        Q = @
else
        Q =
endif

CXX = g++ -std=c++11
OSNAME = Linux

CODA     = /daqfs/coda/3.07
CODALIB  = ${CODA}/Linux-x86_64/lib
CODALIBS = -L. -L$(CODALIB) -levio -levioxx -lexpat

CODAINC  = ${CODA}/common/include

ROOTSYS	?= /apps/root/5.34.36/root

ROOTLIBS = $(shell ${ROOTSYS}/bin/root-config --libs)
ROOTGLIBS = $(shell ${ROOTSYS}/bin/root-config --glibs)
ROOTINC   = $(shell ${ROOTSYS}/bin/root-config --incdir)

OTHERLIBS = -lieee -lrt -lpthread -lm -lnsl -lresolv -ldl

LIBS = $(ROOTLIBS) $(ROOTGLIBS) $(CODALIBS) $(OTHERLIBS)

INCLUDES      = -I$(ROOTINC) \
		-I${CODAINC}

CXXFLAGS = -fPIC -D_REENTRANT -D_GNU_SOURCE $(INCLUDES)
ifdef DEBUG
CXXFLAGS += -g -Wall
else
CXXFLAGS += -O3
endif

SRC	= $(filter-out AnaDict.C, $(wildcard *.C))
SRC	:= $(filter-out rootlogon.C, $(SRC))
OBJ	= $(SRC:.C=.o) AnaDict.o
DEPS	= $(SRC:.C=.d)
HDRS	= $(filter-out AnaDict.h, $(wildcard *.h))

all: tdana libtdana.so

tdana: ${OBJ}
	@echo " CC     $@"
	${Q}$(CXX) $(CXXFLAGS) -o $@ ${OBJ} $(LIBS)

%.o: %.C
	@echo " CC     $@"
	${Q}$(CXX) $(CXXFLAGS) -c $<

AnaDict.C: ${HDRS} 
	@echo " DICT   $@"
	${Q}$(ROOTSYS)/bin/rootcint -f $@ -c -p $(INCLUDES) ${HDRS} 

libtdana.so: AnaDict.C ${filter-out main.C, ${SRC}}
	@echo " CC     $@"
	${Q}$(CXX) $(CXXFLAGS) -fpic -shared -o $@ ${LIBS} $^

clean: 
	rm -f *.{o,d} *.d.* core *~ AnaDict.{C,h}

show:
	@echo " SRC  = $(SRC)"
	@echo " OBJ  = $(OBJ)"
	@echo " HDRS = $(HDRS)"

%.d: %.C
	@echo " DEP    $@"
	@set -e; rm -f $@ $@.*; \
	$(CXX) -MM -shared $(INCLUDES) $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

-include $(DEPS)
