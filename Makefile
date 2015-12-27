# Recursive automatic Makefile
# @author Jorma Rebane
# @date 20.01.2015
# Recursively compiles all source in src/ into a static library
# And then recursively compiles the test binaries
#
# Written for MinGW
make_unique = $(if $1,$(firstword $1) $(call make_unique,$(filter-out $(firstword $1),$1)))
find = find
ifeq ($(OS),Windows_NT)
	find=C:/MinGW/msys/1.0/bin/find.exe
endif
ifeq ($(OS),Windows_NT)
	TESTOUT = bin/test.exe
	LIB = bin/libws2_32.a
else
	TESTOUT = bin/test
	LIB = -pthread
endif

LIBSRCS   := $(shell $(find) rpp/   -iname *.cpp)
TESTSRCS  := $(shell $(find) tests/ -iname *.cpp)
LIBOBJS   := $(LIBSRCS:%.cpp=obj/%.o)
TESTOBJS  := $(TESTSRCS:%.cpp=obj/%.o)
AUTODEPS  := $(LIBSRCS:%.cpp=obj/%.d) $(TESTSRCS:%.cpp=obj/%.d)
# /\ hack for windows, %\=% hack for mkdir
OBJDIRS   := $(subst /,\,$(dir $(AUTODEPS)))
OBJDIRS   := obj obj\rpp $(OBJDIRS:%\=%)
OBJDIRS   := $(call make_unique,$(OBJDIRS))
LIBOUT  := bin/recpp.a
MODE    := -m32
CC      := g++
OPTS    := -g
#MODE    := -m64
#CC      := g++64
#OPTS    := -O3
LFLAGS  := $(MODE) $(OPTS) -Wall
CFLAGS  := $(MODE) $(OPTS) -Wall -Wfatal-errors -Wno-pmf-conversions -std=gnu++1y  -I. -I./rpp/


.phony: all clean
all: $(TESTOUT)
clean:
	@echo Cleaning targets...
	@rm -rf obj/ $(LIBOUT) $(TESTOUT)
test: all
	@echo Running tests...
	./$(TESTOUT)
	
# declare all the autodeps
-include $(AUTODEPS)

$(TESTOUT): obj/ bin/ $(LIBOUT) $(TESTOBJS)
	@echo Linking $(TESTOUT)...
	@$(CC) $(LFLAGS) -o $(TESTOUT) $(TESTOBJS) $(LIBOUT) $(LIB)
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
