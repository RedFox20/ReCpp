# Recursive automatic Makefile
# @author Jorma Rebane
# @date 20.01.2015
# Recursively compiles all source in src/ into a static library
# And then recursively compiles the test binaries
#
# Written for MinGW
make_unique = $(if $1,$(firstword $1) $(call make_unique,$(filter-out $(firstword $1),$1)))
LIBSRCS   := $(shell find rpp/   -iname *.cpp)
TESTSRCS  := $(shell find tests/ -iname *.cpp)
LIBOBJS   := $(LIBSRCS:%.cpp=obj/%.o)
TESTOBJS  := $(TESTSRCS:%.cpp=obj/%.o)
AUTODEPS  := $(LIBSRCS:%.cpp=obj/%.d) $(TESTSRCS:%.cpp=obj/%.d)
# /\ hack for windows, %\=% hack for mkdir
OBJDIRS   := $(subst /,\,$(dir $(AUTODEPS)))
OBJDIRS   := obj obj\rpp $(OBJDIRS:%\=%)
OBJDIRS   := $(call make_unique,$(OBJDIRS))
LIBOUT  := bin/recpp.a
TESTOUT := test.exe
MODE    := -m32
CC      := g++
OPTS    := -g
#MODE    := -m64
#CC      := g++64
#OPTS    := -O3
LFLAGS  := $(MODE) $(OPTS) -Wall
CFLAGS  := $(MODE) $(OPTS) -Wall -Wfatal-errors -Wno-pmf-conversions -std=c++11 -I./ -I./rpp/


.phony: all clean
all: $(TESTOUT)
clean:
	@echo Cleaning targets...
	@rm -rf obj/ $(LIBOUT) $(TESTOUT)
	
# declare all the autodeps
-include $(AUTODEPS)

$(TESTOUT): obj/ bin/ $(LIBOUT) $(TESTOBJS)
	@echo Linking $(TESTOUT)...
	@$(CC) $(LFLAGS) -o $(TESTOUT) $(TESTOBJS) $(LIBOUT)
obj/%.o: %.cpp
	@echo Compile $*.cpp...
	@$(CC) $(CFLAGS) -c $*.cpp -o obj/$*.o -MD

$(LIBOUT): $(LIBOBJS)
	@echo Linking $(LIBOUT)
	@ar rcs $(LIBOUT) $(LIBOBJS)
obj:
	@mkdir $(OBJDIRS)
bin:
	@mkdir bin
