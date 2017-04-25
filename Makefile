# Compile the fcat analysis.
# Bryan Moffit, August 2003
# DEBUG=1

CXX = g++ -std=c++11
OSNAME = Linux

CODA     = /daqfs/coda/3.06
CODALIB  = ${CODA}/Linux-x86_64/lib
CODALIBS = -L. -L$(CODALIB) -levio -levioxx -lexpat

CODAINC  = ${CODA}/common/include

ROOTLIBS = $(shell ${ROOTSYS}/bin/root-config --libs)
ROOTGLIBS = $(shell ${ROOTSYS}/bin/root-config --glibs)
ROOTINC   = $(shell ${ROOTSYS}/bin/root-config --incdir)

CMSGLIBS = -L${CODA}//Linux-x86_64/lib -lcmsg -lcmsgxx -lcmsgRegex -lieee \
	-lrt -lpthread -lm -lnsl -lresolv -ldl

LIBS = $(ROOTLIBS) $(ROOTGLIBS) $(CODALIBS) $(CMSGLIBS)

INCLUDES      = -I$(ROOTINC) \
		-I${CODAINC}

CXXFLAGS = -fPIC -D_REENTRANT -D_GNU_SOURCE $(INCLUDES)
ifdef DEBUG
CXXFLAGS += -g -Wall
else
CXXFLAGS += -O3
endif

all: fcat libfcat.so

fcat: main.o FCATAnalysis.o FCATcmsg.o FCATAnalysis.h AnaDict.o
	$(CXX) $(CXXFLAGS) -o $@ main.o FCATAnalysis.o FCATcmsg.o  AnaDict.o $(LIBS)
	@echo
	mv fcat ../

main.o: main.C 
	$(CXX) $(CXXFLAGS) -c main.C
	@echo

FCATAnalysis.o: FCATAnalysis.C FCATAnalysis.h
	$(CXX) $(CXXFLAGS) -c FCATAnalysis.C
	@echo

FCATcmsg.o: FCATcmsg.C FCATcmsg.h
	$(CXX) $(CXXFLAGS) -c FCATcmsg.C
	@echo

AnaDict.o: FCATAnalysis.h FCATcmsg.h LinkDef.h AnaDict.C
	$(CXX) $(CXXFLAGS) -c AnaDict.C
	@echo

AnaDict.C: FCATAnalysis.h FCATcmsg.h LinkDef.h
	@echo "Generating Decoder Dictionary..."
	$(ROOTSYS)/bin/rootcint -f AnaDict.C -c -p $(INCLUDES) FCATAnalysis.h FCATcmsg.h LinkDef.h
	@echo

libfcat.so: FCATAnalysis.o FCATcmsg.o AnaDict.o
	$(CXX) -shared -O -o libfcat.so FCATAnalysis.o FCATcmsg.o  AnaDict.o \
		$(LIBS)
	@echo

clean: 
	rm -f *.o core fcat *~ AnaDict.{C,h} libfcat.so
