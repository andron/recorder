# -*- mode:make; tab-width:2; -*-

include $(HEADER)

# --------------------------------------------------
NAME     := zmqtest
VERSION  := 1.0.0
RELEASE  := 1
REQUIRES := #
$(call setup)
# --------------------------------------------------

_CXXFLAGS := -std=c++11
_LDFLAGS  := -rdynamic -Wl,-rpath=../lib -Wl,-rpath=./lib -Wl,-rpath=$(TGTDIR)

TARGETS := zmqtest

zmqtest_SRCS := src/*.cpp src/proto/*.cc
zmqtest_USES := zeromq protobuf
zmqtest_LINK := zmq protobuf pthread

include $(FOOTER)
