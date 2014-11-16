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
#include "RecorderBase.h"

#include "zmqutils.h"

#include <zmq.hpp>

#include <chrono>
#include <map>

typedef std::chrono::milliseconds msec;
typedef std::chrono::microseconds usec;

RecorderHDF5::RecorderHDF5()
    : RecorderBase("HDF5Backend") {
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
  zmq::socket_t sock(*RecorderBase::socket_context, ZMQ_PULL);
  zmqutils::bind(&sock, RecorderBase::socket_address.c_str());
  zmq::message_t zmsg;
  bool messages_to_process = true;
  std::array<int32_t, 4096> counter;
  int64_t count = 0;
  zmq_pollitem_t pollitems[] = { { sock, 0, ZMQ_POLLIN, 0 } };
  while (poller_running_.load() || messages_to_process) {
    if (!zmqutils::poll(pollitems)) {
      messages_to_process = false;
      continue;
    }

    auto type = zmqutils::pop<PayloadType>(&sock, &zmsg);

    switch (type) {
      case PayloadType::DATA: {
        sock.recv(&zmsg);
        auto num_params = zmsg.size() / sizeof(Item);
        count += num_params;
        for (size_t i = 0; i < num_params; ++i) {
          auto* item = static_cast<Item*>(zmsg.data()) + i;
          int32_t rec_id = item->recorder_id;
          counter[rec_id] += 1;
        }
      } break;;
      case PayloadType::INIT_ITEM: {
        auto init = zmqutils::pop<InitItem>(&sock, &zmsg);
        printf("(ITEM): %4d-%d '%s' '%s' '%s'\n",
               init.recorder_id,
               init.key,
               init.name,
               init.unit,
               init.desc);
      } break;;
      case PayloadType::INIT_RECORDER: {
        auto init = zmqutils::pop<InitRecorder>(&sock, &zmsg);
        printf("(REC):  %4d(%ld) L%d '%s'\n",
               init.recorder_id,
               init.external_id,
               init.recorder_num_items,
               init.recorder_name);
        // int32_t rec_id = init.recorder_id;
        // counter.reserve(rec_id);
      } break;;
      default:
        break;;
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

  for (size_t i = 0; i < counter.max_size(); ++i) {
    if (counter[i] > 0)
      printf("(RECV): %2lu:%d\n", i, counter[i]);
  }
}
