# Makefile for libdialer
# Copyright (C) 2014 Sergey Kolevatov

###################################################################

include Makefile.inc

VER := 0

MODE ?= debug

###################################################################

BOOST_PATH := $(shell echo $$BOOST_PATH)

ifeq (,$(BOOST_PATH))
    $(error 'please define path to boost $$BOOST_PATH')
endif

###################################################################

BOOST_INC=$(BOOST_PATH)
BOOST_LIB_PATH=$(BOOST_PATH)/stage/lib

BOOST_LIB_NAMES := regex
BOOST_LIBS = $(patsubst %,$(BOOST_LIB_PATH)/libboost_%.a,$(BOOST_LIB_NAMES))


###################################################################
GDK_LIB=$(shell pkg-config --libs dbus-1)
EXT_LIBS=$(GDK_LIB) $(BOOST_LIBS)

###################################################################

PROJECT := dialer

LIBNAME=lib$(PROJECT)

###################################################################

ifeq "$(MODE)" "debug"
    OBJDIR=./DBG
    BINDIR=./DBG

    CFLAGS := -Wall -std=c++0x -ggdb -g3
    LFLAGS := -Wall -lstdc++ -lrt -ldl -lm -g
    LFLAGS_TEST := -Wall -lstdc++ -lrt -ldl -g -L. $(BINDIR)/$(LIBNAME).a -lm -pthread

    TARGET=example
else
    OBJDIR=./OPT
    BINDIR=./OPT

    CFLAGS := -Wall -std=c++0x
    LFLAGS := -Wall -lstdc++ -lrt -ldl -lm
    LFLAGS_TEST := -Wall -lstdc++ -lrt -ldl -L. $(BINDIR)/$(LIBNAME).a -lm -pthread

    TARGET=example
endif

###################################################################

INCL = -I$(BOOST_INC) -I.
#INCL = -I.


STATICLIB=$(LIBNAME).a

SRCC = dialer.cpp regex_match.cpp str_helper.cpp player_sm.cpp
OBJS = $(patsubst %.cpp,$(OBJDIR)/%.o,$(SRCC))

LIB_NAMES = skype_service skype_io scheduler utils
LIBS = $(patsubst %,$(BINDIR)/lib%.a,$(LIB_NAMES))

all: static

static: $(TARGET)

check: test

test: all teststatic

teststatic: static
	@echo static test is not ready yet, dc10

$(BINDIR)/$(STATICLIB): $(OBJS)
	$(AR) $@ $(OBJS)
	-@ ($(RANLIB) $@ || true) >/dev/null 2>&1

$(OBJDIR)/%.o: %.cpp
	@echo compiling $<
	$(CC) $(CFLAGS) -DPIC -c -o $@ $< $(INCL)

$(TARGET): $(BINDIR) $(BINDIR)/$(TARGET)
	ln -sf $(BINDIR)/$(TARGET) $(TARGET)
	@echo "$@ uptodate - ${MODE}"

$(BINDIR)/$(TARGET): $(OBJDIR)/$(TARGET).o $(OBJS) $(BINDIR)/$(STATICLIB) $(LIB_NAMES)
	$(CC) $(CFLAGS) -o $@ $(OBJDIR)/$(TARGET).o $(BINDIR)/$(LIBNAME).a $(LIBS) $(EXT_LIBS) $(LFLAGS_TEST)

$(LIB_NAMES):
	make -C ../$@
	ln -sf ../../$@/$(BINDIR)/lib$@.a $(BINDIR)

$(BINDIR):
	@ mkdir -p $(OBJDIR)
	@ mkdir -p $(BINDIR)

clean:
	#rm $(OBJDIR)/*.o *~ $(TARGET)
	rm $(OBJDIR)/*.o $(TARGET) $(BINDIR)/$(TARGET) $(BINDIR)/$(STATICLIB)

cleanall: clean

.PHONY: all $(LIB_NAMES)
