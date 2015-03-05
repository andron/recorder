// -*- mode:c++; indent-tabs-mode:nil; -*-

/*
  Copyright (c) 2014, 2015, Anders Ronnbrant, anders.ronnbrant@gmail.com

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

#include "RecorderSink.h"
#include "RecorderBase.h"

#include "zmqutils.h"

#include <zmq.hpp>

#include <chrono>
#include <map>
#include <sstream>
#include <string>

typedef std::chrono::milliseconds msec;
typedef std::chrono::microseconds usec;

RecorderSink::RecorderSink()
    : RecorderBase("Backend") {
}

RecorderSink::~RecorderSink() {
  if (poller_running_.load()) {
    stop();
  }
}

void
RecorderSink::start(bool verbose) {
  verbose_mode_.store(verbose);
  poller_running_.store(true);
  poller_thread_ = std::thread(&RecorderSink::run, this);
}

void
RecorderSink::stop() {
  poller_running_.store(false);
  if (poller_thread_.joinable()) {
    poller_thread_.join();
  }
}

std::string
itemToString(Item const& item) {
  char buf[64];
  switch (item.type) {
    case ItemType::INT: {
      int64_t const* d = item.data.v_i;
      switch (item.length) {
        case 1:
          snprintf(buf, sizeof(buf), "%ld", d[0]);
          break;;
        case 2:
          snprintf(buf, sizeof(buf), "%ld,%ld", d[0], d[1]);
          break;;
        case 3:
          snprintf(buf, sizeof(buf), "%ld,%ld,%ld", d[0], d[1], d[2]);
          break;;
      }
    } break;;
    case ItemType::UINT: {
      uint64_t const* d = item.data.v_u;
      switch (item.length) {
        case 1:
          snprintf(buf, sizeof(buf), "%lu", d[0]);
          break;;
        case 2:
          snprintf(buf, sizeof(buf), "%lu,%lu", d[0], d[1]);
          break;;
        case 3:
          snprintf(buf, sizeof(buf), "%lu,%lu,%lu", d[0], d[1], d[2]);
          break;;
      }
    } break;;
    case ItemType::FLOAT: {
      double const* d = item.data.v_d;
      switch (item.length) {
        case 1:
          snprintf(buf, sizeof(buf), "%f", d[0]);
          break;;
        case 2:
          snprintf(buf, sizeof(buf), "%f,%f", d[0], d[1]);
          break;;
        case 3:
          snprintf(buf, sizeof(buf), "%f,%f,%f", d[0], d[1], d[2]);
          break;;
      }
    } break;;
    default:
      break;;
  }
  return std::string(buf);
}

void
RecorderSink::run() {
  zmq::socket_t sock(*RecorderBase::socket_context, ZMQ_PULL);
  int constexpr recvhvm = 16000;
  sock.setsockopt(ZMQ_RCVHWM, &recvhvm, sizeof(recvhvm));
  zmqutils::bind(&sock, RecorderBase::socket_address.c_str());

  zmq::message_t zmsg;
  bool messages_to_process = true;
  std::array<int32_t, 4096> counter;
  counter.fill(0);
  int64_t count = 0;
  zmq_pollitem_t pollitems[] = { { sock, 0, ZMQ_POLLIN, 0 } };

  auto t1 = std::chrono::high_resolution_clock::now();

  while (poller_running_.load() || messages_to_process) {
    if (!zmqutils::poll(pollitems)) {
      messages_to_process = false;
      continue;
    }

    auto type = zmqutils::pop<PayloadType>(&sock, &zmsg);

    switch (type) {
      case PayloadType::DATA: {
        auto rcid = zmqutils::pop<int16_t>(&sock, &zmsg);
        sock.recv(&zmsg);
        auto const num_params = zmsg.size() / sizeof(Item);
        count += num_params;
        counter[rcid] += num_params;
        if (verbose_mode_.load()) {
          for (size_t i = 0; i < num_params; ++i) {
            auto const* item = static_cast<Item*>(zmsg.data()) + i;
            auto const str = itemToString(*item);
            printf("(DATA): @%03d %6d-%d T%d L%d -- %s\n",
                   item->time,
                   rcid,
                   item->key,
                   item->type,
                   item->length,
                   str.c_str());
          }
        }
      } break;;
      case PayloadType::INIT_ITEM: {
        auto init = zmqutils::pop<InitItem>(&sock, &zmsg);
        if (verbose_mode_.load()) {
          printf("(ITEM): %6d-%d '%s' '%s'\n",
                 init.recorder_id,
                 init.key,
                 init.name,
                 init.desc);
        }
      } break;;
      case PayloadType::INIT_RECORDER: {
        auto const pkg = zmqutils::pop<InitRecorder>(&sock, &zmsg);
        if (verbose_mode_.load()) {
          printf("(REC):  %4d(%ld) L%d '%s'\n",
                 pkg.recorder_id,
                 pkg.external_id,
                 pkg.recorder_num_items,
                 pkg.recorder_name);
        }
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

  int total = 0;
  for (size_t i = 0; i < counter.max_size(); ++i) {
    if (counter[i] == 0) {
      continue;
    }
    printf("(RECV): %2lu:%d\n", i, counter[i]);
    total += counter[i];
  }
  printf("(RECV): %d\n", total);
}
