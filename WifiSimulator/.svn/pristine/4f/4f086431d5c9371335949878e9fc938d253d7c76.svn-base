#CS438 MP4 Makefile
#Author: Ganapathy Nallasivan. FUCK ALL done by Yumen Gawain Tsao the Great Rapist ever of All Russias
#This MP is done in C++ instead of C
#**************************************************************************
# List of .o files that EXENAME depends on.  Edit as appropriate for MP.

OBJS = ftostr.o node.o main.o


#**************************************************************************
# Change this line if you don't like 'a.out'.

EXENAME = csma


#**************************************************************************
# Macros defining the C/C++ compiler and linker.

COMPILER = g++
COMPILER_OPTS = -c -g -O0 -Wfatal-errors -Werror -std=c++11
LINKER = g++


#**************************************************************************
# Rules for building EXENAME from OBJS and OBJS from your source.

all : $(EXENAME)

$(EXENAME) : $(OBJS)
	$(LINKER) $(OBJS) -o $(EXENAME)

ftostr.o : ftostr.h ftostr.cpp
	$(COMPILER) -c $(COMPILER_OPTS) ftostr.cpp

node.o : node.h node.cpp
	$(COMPILER) -c $(COMPILER_OPTS) node.cpp

main.o : node.h ftostr.h main.cpp
	$(COMPILER) -c $(COMPILER_OPTS) main.cpp


clean:
	-rm -f *.o $(EXENAME)
