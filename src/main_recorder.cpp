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
typedef std::chrono::nanoseconds  nsec;
}

enum class FOO { A, B, C, D, E, X, Y, Z, Count };
enum class BAR { A, B, C, D, E, X, Y, Z, Count };
enum class FUU { A, B, C, D, E, X, Y, Z, Count };
enum class BER { A, B, C, D, E, X, Y, Z, Count };

template<typename T>
void
funcSetupRecorder(Recorder<T>& rec, char const* prefix, int const id) {
  char name[8][12];

  snprintf(name[0], 12, "%s-chr%02d", prefix, id);
  snprintf(name[1], 12, "%s-dbl%02d", prefix, id);
  snprintf(name[2], 12, "%s-i32%02d", prefix, id);
  snprintf(name[3], 12, "%s-i64%02d", prefix, id);
  snprintf(name[4], 12, "%s-uns%02d", prefix, id);
  snprintf(name[5], 12, "%s-af3%02d", prefix, id);
  snprintf(name[6], 12, "%s-ad3%02d", prefix, id);
  snprintf(name[7], 12, "%s-id2%02d", prefix, id);

  rec.setup(T::A, name[0], "m");
  rec.setup(T::B, name[1], "s");
  rec.setup(T::C, name[2], "C");
  rec.setup(T::D, name[3], "u");
  rec.setup(T::E, name[4], "g");
  rec.setup(T::X, name[5], "-");
  rec.setup(T::Y, name[6], "-");
  rec.setup(T::Z, name[7], "-");
}

template<typename T>
void
funcRecordRecorder(Recorder<T>& rec, int x) {
  float  data1[3] = {500.0f*x, 600.0f*x, 700.0f*x};
  double data2[3] = {500.0*x, 600.0*x, 700.0*x};
  rec.record(T::A, *reinterpret_cast<char*>(&x));
  rec.record(T::B, std::log(1+x));
  rec.record(T::C, reinterpret_cast<int32_t>(-x*x));
  rec.record(T::D, static_cast<int64_t>(-x*x));
  rec.record(T::E, static_cast<uint64_t>(x*x));
  rec.record(T::X, data1);
  rec.record(T::Y, data2);
  rec.record(T::Z, {10*x, 20*x});
}


void funcProducer(int const id, int const num_rounds) {
  std::string prefix;
  char recorder_name[32];

  prefix = "FOO";
  snprintf(recorder_name, sizeof(recorder_name), "%s%02d", prefix.c_str() , id);
  Recorder<FOO> recfoo(recorder_name, random() % (1<<16));
  funcSetupRecorder(recfoo, prefix.c_str(), id);

  prefix = "BAR";
  snprintf(recorder_name, sizeof(recorder_name), "%s%02d", prefix.c_str() , id);
  Recorder<BAR> recbar(recorder_name, random() % (1<<16));
  funcSetupRecorder(recbar, prefix.c_str(), id);

  prefix = "FUU";
  snprintf(recorder_name, sizeof(recorder_name), "%s%02d", prefix.c_str() , id);
  Recorder<FUU> recfuu(recorder_name, random() % (1<<16));
  funcSetupRecorder(recfuu, prefix.c_str(), id);

  prefix = "BER";
  snprintf(recorder_name, sizeof(recorder_name), "%s%02d", prefix.c_str() , id);
  Recorder<BER> recber(recorder_name, random() % (1<<16));
  funcSetupRecorder(recber, prefix.c_str(), id);

  for (int j = 0; j < num_rounds; ++j) {
    std::this_thread::sleep_for(nsec(10));
    funcRecordRecorder(recfoo, j);
    funcRecordRecorder(recbar, j);
    funcRecordRecorder(recfuu, j);
    funcRecordRecorder(recber, j);
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

  int num_rounds = 1e3;
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
