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

namespace {
typedef std::chrono::milliseconds msec;
}

int
main(int, char**) {
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

  std::thread puller = std::thread(
      [&ctx, &running, addr] {
        zmq::socket_t sock(ctx, ZMQ_PULL);
        zmqutils::bind(&sock, addr);
        zmq::message_t zmsg(128);
        bool messages_to_process = true;
        int count = 0;
        zmq_pollitem_t pollitems[] = { { sock, 0, ZMQ_POLLIN, 0 } };
        while (running.load() || messages_to_process) {
          if (!zmqutils::poll(pollitems)) {
            messages_to_process = false;
          } else {
            sock.recv(&zmsg);
            ++count;
          }
        }
        sock.close();
        printf("msg count: %d\n", count);
      });

  Recorder::setContext(&ctx);
  Recorder::setAddress(addr);

  std::vector<std::thread> recorders;
  for (int i = 0; i < 4; ++i) {
    recorders.emplace_back(std::thread(
        [&i] {
          char name1[8];
          char name2[8];
          snprintf(name1, sizeof(name1), "foo%02d", i);
          snprintf(name2, sizeof(name2), "bar%02d", i);

          Recorder rec(i);
          rec.setup(name1, "m");
          rec.setup(name2, "ms");

          for (int j = 0; j < 1e5; ++j) {
            rec.record(name1, j);
            rec.record(name2, 1.0/j);
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
