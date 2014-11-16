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

#include "RecorderHDF5.h"

#include "zmqutils.h"

#include <zmq.hpp>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <string>
#include <thread>
#include <vector>

namespace {
typedef std::chrono::milliseconds msec;
typedef std::chrono::microseconds usec;
}

enum class FOO { A, B, C, D, X, Y, Count };

void funcProducer(int const id, int const num_rounds) {
  char recorder_name[32];
  char name1[8];
  char name2[8];
  char name3[8];
  char name4[8];
  snprintf(recorder_name, sizeof(recorder_name), "REC%04d", id);
  snprintf(name1, sizeof(name1), "A%02d", id);
  snprintf(name2, sizeof(name2), "B%02d", id);
  snprintf(name3, sizeof(name3), "C%02d", id);
  snprintf(name4, sizeof(name4), "D%02d", id);

  Recorder<FOO> rec(recorder_name, id*100);
  rec.setup(FOO::A, name1, "m");
  rec.setup(FOO::B, name2, "ms");
  rec.setup(FOO::C, name3, "kg");
  rec.setup(FOO::D, name4, "m/s");
  rec.setup(FOO::X, "testX", "xs");
  rec.setup(FOO::Y, "testY", "ys");

  for (int j = 0; j < num_rounds; ++j) {
    rec.record(FOO::A, *reinterpret_cast<char*>(&j));
    rec.record(FOO::B, 1.0/j);
    rec.record(FOO::C, j*j);
    rec.record(FOO::D, std::log(j));

    double data[3] = {500.0*j*id, 600.0*j*id, 700.0*j*id};
    rec.record(FOO::X, data);
    rec.record(FOO::Y, {10*j*id, 20*j*id});
  }
}


int
main(int ac, char** av) {
  zmq::context_t ctx(1);

  std::string const addr("inproc://recorder");

  RecorderBase::setContext(&ctx);
  RecorderBase::setAddress(addr);

  printf("Item size: %lu\n", sizeof(Item));

  RecorderHDF5 backend;
  backend.start();

  int num_threads  = 4;
  if (ac > 1)
    num_threads = std::atoi(av[1]);

  int num_rounds = 1e5;
  if (ac > 2)
    num_rounds = std::atoi(av[2]);

  auto num_messages = num_threads * 6 * (2 * num_rounds - 1);
  printf("Running %d threads, sending 6*%d records -> %d (%ldMiB)\n",
         num_threads, num_rounds, num_messages,
         (num_messages * sizeof(Item))/(1024*1024));

  std::vector<std::thread> recorders;
  for (int i = 0; i < num_threads; ++i) {
    recorders.emplace_back(std::thread(&funcProducer, i+1, num_rounds));
  }

  for (auto& th : recorders) {
    if (th.joinable())
      th.join();
  }

  std::this_thread::sleep_for(msec(1));

  backend.stop();

  return 0;
}
