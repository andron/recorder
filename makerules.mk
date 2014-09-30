# -*- mode:make; tab-width:2; -*-

include $(HEADER)

# --------------------------------------------------
NAME     := zmqtest
VERSION  := 1.0.0
RELEASE  := 1
REQUIRES := #
$(call setup)
# --------------------------------------------------

_CXXFLAGS := -std=c++11 -O0
_LDFLAGS  := -rdynamic -Wl,-rpath=../lib -Wl,-rpath=./lib -Wl,-rpath=$(TGTDIR)

TARGETS := recordertest servertest

recordertest_SRCS := src/main_recorder.cpp src/Recorder.cpp src/proto/*.cc
recordertest_USES := zeromq protobuf
recordertest_LINK := zmq protobuf pthread

servertest_SRCS := src/main_server.cpp
servertest_USES := zeromq
servertest_LINK := zmq pthread

include $(FOOTER)
