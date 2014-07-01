# -*- mode:make; tab-width:2; -*-

include $(HEADER)

SOFTWARE_HOMES := /home/andro/git/install

# --------------------------------------------------
NAME     := zmqtest
VERSION  := 1.0.0
RELEASE  := 1
REQUIRES := zeromq
$(call setup)
# --------------------------------------------------

_CXXFLAGS := -std=c++11
_LDFLAGS  := -rdynamic -Wl,-rpath=../lib -Wl,-rpath=./lib -Wl,-rpath=$(TGTDIR) -Wl,-rpath=$(ZEROMQ_HOME)/lib

TARGETS := zmqtest

zmqtest_SRCS := src/*.cpp
zmqtest_USES := zeromq
zmqtest_LINK := zmq pthread

include $(FOOTER)
