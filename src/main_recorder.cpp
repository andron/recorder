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

#include "Recorder.h"

#include "zmqutils.h"

#include "proto/test.pb.h"

#include <zmq.hpp>

#include <string>
#include <atomic>
#include <thread>
#include <cstdlib>
#include <chrono>

namespace {
typedef std::chrono::milliseconds msec;
typedef std::chrono::microseconds usec;
}

int
main(int ac, char** av) {
  zmq::context_t ctx(1);

  char* buf = reinterpret_cast<char*>(malloc(32*1e6));

  for (int i = 0; i < 3e5; ++i) {
    Position p;
    std::string str;
    p.set_time(i * 0.1);
    p.set_x(i * 1.0);
    p.set_y(i * 2.0);
    p.set_z(i * 4.0);
    p.SerializeToString(&str);
    memcpy(buf + i * p.ByteSize(), str.c_str(), str.length());
  }

  std::string const addr("inproc://recorder");
  std::atomic<bool> running(true);

  float duration_msec;

  std::thread puller = std::thread(
      [&ctx, &running, &duration_msec, addr] {
        auto t1 = std::chrono::high_resolution_clock::now();
        zmq::socket_t sock(ctx, ZMQ_PULL);
        zmqutils::bind(&sock, addr);
        zmq::message_t zmsg(256);
        bool messages_to_process = true;
        int64_t count = 0;
        zmq_pollitem_t pollitems[] = { { sock, 0, ZMQ_POLLIN, 0 } };
        while (running.load() || messages_to_process) {
          if (!zmqutils::poll(pollitems)) {
            messages_to_process = false;
          } else {
            sock.recv(&zmsg);
            auto num_params = zmsg.size() / sizeof(Recorder::Item);
            count += num_params;

            Recorder::Item* item = reinterpret_cast<Recorder::Item*>(zmsg.data());
            if (item->type == Recorder::Item::Type::STR) {
              printf("Value: %s\n", item->data.s);
            }
          }
        }
        sock.close();

        auto mib = 1<<20;

        auto t2 = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<usec>(t2 - t1);
        duration_msec = duration.count()/1000.0;
        printf("Messages:     %ld (%.3fms)\n", count, duration_msec);
        printf("Messages/sec: %.1f (%.1fMiB/sec)\n",
               count * 1000 / duration_msec,
               sizeof(Recorder::Item) * count * 1000 / (mib * duration_msec));
      });

  Recorder::setContext(&ctx);
  Recorder::setAddress(addr);

  {
    Recorder foo(0);
    foo.setup("test","m/s");
    foo.record("test","foppa");
    foo.record("test","nalle");
    foo.record("test","fludo");
    foo.flushSendBuffer();
  }

  int num_threads  = 4;
  if (ac > 1)
    num_threads = std::atoi(av[1]);

  int num_rounds = 1e5;
  if (ac > 2)
    num_rounds = std::atoi(av[2]);

  auto num_messages = num_threads * 4 * (2 * num_rounds - 1);
  printf("Running %d threads, sending 4*%d records -> %d (%ldMiB)\n",
         num_threads, num_rounds, num_messages,
         (num_messages * sizeof(Recorder::Item))/(1024*1024));

  std::vector<std::thread> recorders;
  for (int i = 0; i < num_threads; ++i) {
    recorders.emplace_back(std::thread(
        [&i, &num_rounds] {
          char name1[8];
          char name2[8];
          char name3[8];
          char name4[8];
          snprintf(name1, sizeof(name1), "A%02d", i);
          snprintf(name2, sizeof(name2), "B%02d", i);
          snprintf(name3, sizeof(name3), "C%02d", i);
          snprintf(name4, sizeof(name4), "D%02d", i);

          Recorder rec(i);
          rec.setup(name1, "m");
          rec.setup(name2, "ms");
          rec.setup(name3, "kg");
          rec.setup(name4, "m/s");

          for (int j = 0; j < num_rounds; ++j) {
            rec.record(name1, j);
            rec.record(name2, 1.0/j);
            rec.record(name3, j*j);
            rec.record(name4, std::log(j));
          }
        }));
  }

  for (auto& th : recorders) {
    if (th.joinable())
      th.join();
  }

  std::this_thread::sleep_for(msec(1));

  running.store(false);

  if (puller.joinable())
    puller.join();

  return 0;
}
