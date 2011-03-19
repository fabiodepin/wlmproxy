CXX = g++
CXXFLAGS = -O2 -g -pipe -Wall -fno-rtti -fno-exceptions
#CXXFLAGS += -march=native
DEFS = -DDEBUG
INCLUDES = \
	-I. \
	-I/usr/include/mysql \
	-Ithird_party/rapidxml
#LDFLAGS = -Wl,-s
LIBS = -levent -lpthread -lcrypto -lboost_regex -ldolphinconn

PROG = wlmproxy

SRC_DIRS = \
	. \
	compat \
	history \
	msn \
	thread

SRCS = $(foreach dir, $(SRC_DIRS), $(wildcard $(dir)/*.cc $(dir)/*.c))
OBJS = $(addsuffix .o, $(basename $(SRCS)))

ifdef V
Q = 
else
Q = @
endif

all: $(PROG)

.SUFFIXES:
.SUFFIXES: .c .cc .o

$(PROG): $(OBJS)
	@echo Linking $@...
	$(Q)$(CXX) $(LDFLAGS) $^ $(LIBS) -o $@

.c.o:
	@echo Compiling $<...
	$(Q)$(CXX) $(DEFS) $(INCLUDES) $(CXXFLAGS) -c $< -o $@

.cc.o:
	@echo Compiling $<...
	$(Q)$(CXX) $(DEFS) $(INCLUDES) $(CXXFLAGS) -c $< -o $@

clean:
	-rm -f $(OBJS) $(PROG)
