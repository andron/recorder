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

_CXXFLAGS := -std=c++11 -pedantic
_LDFLAGS  := \
	-rdynamic \
	-Wl,-z,origin \
	-Wl,-rpath=\$$$$ORIGIN/../lib \
	-Wl,-rpath=$(TGTDIR) \
	-Wl,-rpath=$(ZEROMQ_HOME)/lib

TARGETS := recordertest

recordertest_SRCS := \
	src/main_recorder.cpp \
	src/RecorderBase.cpp \
	src/RecorderTypes.cpp \
	src/RecorderSink.cpp

recordertest_USES := zeromq protobuf
recordertest_LINK := zmq protobuf pthread boost_program_options

include $(FOOTER)
