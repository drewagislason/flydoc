#  Makefile for flymake
#  Copyright (c) 2024 Drew Gislason, All Rights Reserved.

UNAME=$(shell uname -s)

ifdef WINDIR
	HOSTENV=Cygwin
	HOSTFLAGS=
	HOST_LFLAGS=-limagehlp
else
	ifeq ($(UNAME),Darwin)
		HOSTENV=Mac
		HOSTFLAGS=
		HOST_LFLAGS=
	else
		HOSTENV=Linux
		HOSTFLAGS=
		HOST_LFLAGS= 
	endif
endif

# common dependancies
DEPS=../../flylibc/inc/Fly.h

# compile local and lib objs the same way
VPATH = .:../../flylibc/lib

# output folder
OUT=out

# include folders
INCLUDE=-I../inc -I../../flylibc/inc

# defines
DEFINES=

CC=cc
CCFLAGS=-Wall -Werror $(HOSTFLAGS) $(INCLUDE) -o
CFLAGS=-c $(DEFINES) $(CCFLAGS)
LFLAGS=$(HOST_LFLAGS) -o

$(OUT)/%.o: %.c $(DEPS)
	$(CC) $< $(CFLAGS) $@

OBJ_FLYDOC = \
	$(OUT)/FlyAssert.o \
	$(OUT)/FlyCli.o \
	$(OUT)/FlyFile.o \
	$(OUT)/FlyFileList.o \
	$(OUT)/FlyList.o \
	$(OUT)/FlyMarkdown.o \
	$(OUT)/FlyMem.o \
	$(OUT)/FlySignal.o \
	$(OUT)/FlyStr.o \
	$(OUT)/FlyStrHdr.o \
	$(OUT)/FlyStrSmart.o \
	$(OUT)/FlyStrZ.o \
	$(OUT)/FlyUtf8.o \
	$(OUT)/flydoccss.o \
	$(OUT)/flydochome.o \
	$(OUT)/flydochtml.o \
	$(OUT)/flydocmd.o \
	$(OUT)/flydocparse.o \
	$(OUT)/flydocprint.o \
	$(OUT)/flydocuserguide.o \
	$(OUT)/flydoc.o

.PHONY: clean mkout SayAll SayDone

all: SayAll mkout flydocuserguide.c flydoc SayDone

SayAll:
	@echo ------------------------------
	@echo Making flydoc...
	@echo ------------------------------

SayDone:
	@echo ------------------------------
	@echo Created flydoc
	@echo ------------------------------

flydoc: mkout $(OBJ_FLYDOC)
	$(CC) $(LFLAGS) $@ $(OBJ_FLYDOC)
	@echo Linked $@ ...

# clean up files that don't need to be checked in to git
# "test_*" are test case executables, "tmp_*" are temporary test case data
clean:
	rm -rf out/
	rm -f *.log
	rm -f flydoc
	rm -f tmp.*

# make the out folder
mkout:
	-if test ! -d $(OUT); then mkdir $(OUT); fi

flydocuserguide.c: ../docs/flydoc_user_guide.md
	file2c ../docs/flydoc_user_guide.md flydocuserguide.c szFlyDocManual
