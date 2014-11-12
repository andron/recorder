# -*- mode:makefile; tab-width:2 -*-

include $(HEADER)

ZEROMQ_HOME := /opt/zeromq/latest

# --------------------------------------------------
NAME     := zmqtest
VERSION  := 1.0.0
RELEASE  := 1
REQUIRES := zeromq
$(call setup)
# --------------------------------------------------

_CXXFLAGS := -std=c++11
_LDFLAGS  := -rdynamic -Wl,-rpath=../lib -Wl,-rpath=./lib -Wl,-rpath=$(TGTDIR)

TARGETS := recordertest servertest foo

recordertest_SRCS := \
	src/main_recorder.cpp \
	src/RecorderBase.cpp \
	src/RecorderItem.cpp \
	src/RecorderHDF5.cpp

recordertest_USES := zeromq protobuf
recordertest_LINK := zmq protobuf pthread

servertest_SRCS := src/main_server.cpp
servertest_USES := zeromq
servertest_LINK := zmq pthread

foo_SRCS := src/main_foo.cpp

include $(FOOTER)
