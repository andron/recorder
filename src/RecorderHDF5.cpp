// -*- mode:c++; indent-tabs-mode:nil; -*-

/*
  Copyright (c) 2014, Anders Ronnbrant, anders.ronnbrant@gmail.com

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  THE SOFTWARE.
*/

#include "RecorderHDF5.h"
#include "Recorder.h"

#include "zmqutils.h"

#include <zmq.hpp>

#include <chrono>

typedef std::chrono::milliseconds msec;
typedef std::chrono::microseconds usec;

RecorderHDF5::RecorderHDF5()
    : RecorderCommon(0) {
}

RecorderHDF5::~RecorderHDF5() {
  if (poller_running_.load()) {
    stop();
  }
}

void
RecorderHDF5::start() {
  printf("Starting ...\n");
  poller_running_.store(true);
  poller_thread_ = std::thread(&RecorderHDF5::run, this);
}

void
RecorderHDF5::stop() {
  printf("Stopping ...\n");
  poller_running_.store(false);
  if (poller_thread_.joinable()) {
    poller_thread_.join();
  }
}

void
RecorderHDF5::run() {
  auto t1 = std::chrono::high_resolution_clock::now();
  zmq::socket_t sock(*RecorderCommon::socket_context, ZMQ_PULL);
  zmqutils::bind(&sock, RecorderCommon::socket_address.c_str());
  zmq::message_t zmsg(1024);
  bool messages_to_process = true;
  int64_t count = 0;
  zmq_pollitem_t pollitems[] = { { sock, 0, ZMQ_POLLIN, 0 } };
  while (poller_running_.load() || messages_to_process) {
    if (!zmqutils::poll(pollitems)) {
      messages_to_process = false;
      continue;
    }

    auto id = zmqutils::pop<uint64_t>(&sock, &zmsg);
    auto type = zmqutils::pop<int16_t>(&sock, &zmsg);

    if (type == 0) {
      sock.recv(&zmsg);
      auto num_params = zmsg.size() / sizeof(Item);
      count += num_params;
    } else if (type == 1) {
      auto init = zmqutils::pop<ItemInit>(&sock, &zmsg);
      printf("(%lu) Setup: '%s', '%s' : \"%s\"\n",
             id, init.name, init.unit, init.desc);
    }

    size_t idx = 0;
    while (idx < (zmsg.size()/sizeof(Item))) {
      ++idx;
    }
  }
  sock.close();

  auto const mib = 1<<20;

  auto t2 = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<usec>(t2 - t1);
  double duration_msec = duration.count()/1000.0;
  printf("Messages:     %ld (%.3fms)\n", count, duration_msec);
  printf("Messages/sec: %.1f (%.1fMiB/sec)\n",
         count * 1000 / duration_msec,
         sizeof(Item) * count * 1000 / (mib * duration_msec));
}
