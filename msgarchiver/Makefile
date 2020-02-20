CC	= gcc

## NOTE: REQUIRES AN ENVIRON VAR SOLCLIENT_DIR=/full/path/to/solclient
SOLCLIENT_DIR ?= ./solclient

INCDIRS = $(SOLCLIENT_DIR)/include .
SYMS    = PROVIDE_LOG_UTILITIES SOLCLIENT_CONST_PROPERTIES _REENTRANT
DEBUG   = -g

LIBDIRS = $(SOLCLIENT_DIR)/lib .
LIBS    = solclient pthread

CPPFLAGS= $(foreach d, $(INCDIRS), -I$d) $(foreach s, $(SYMS), -D$s) -m64 $(DEBUG) 
# -Woverloaded-virtual -fmessage-length=0
LDFLAGS = $(foreach d, $(LIBDIRS), -L$d) $(foreach l, $(LIBS), -l$l)

msgarchiver: msgarchiver.cpp connector.cpp sol_props.cpp

clean:
	$(RM) -rf  msgarchiver msgarchiver.dSYM
