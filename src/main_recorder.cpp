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

#include <boost/program_options.hpp>

#include <zmq.hpp>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <string>
#include <thread>
#include <vector>

namespace po = boost::program_options;

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

  snprintf(name[0], sizeof(name[0]), "%s-chr%02d", prefix, id);
  snprintf(name[1], sizeof(name[1]), "%s-dbl%02d", prefix, id);
  snprintf(name[2], sizeof(name[2]), "%s-i32%02d", prefix, id);
  snprintf(name[3], sizeof(name[3]), "%s-i64%02d", prefix, id);
  snprintf(name[4], sizeof(name[4]), "%s-uns%02d", prefix, id);
  snprintf(name[5], sizeof(name[5]), "%s-af3%02d", prefix, id);
  snprintf(name[6], sizeof(name[6]), "%s-ad3%02d", prefix, id);
  snprintf(name[7], sizeof(name[7]), "%s-id2%02d", prefix, id);

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
  rec.record(T::A, *reinterpret_cast<char*>(&x), x);
  rec.record(T::B, std::log(1+x), x);
  rec.record(T::C, reinterpret_cast<int32_t>(-x*x), x);
  rec.record(T::D, static_cast<int64_t>(-x*x), x);
  rec.record(T::E, static_cast<uint64_t>(x*x), x);
  rec.record(T::X, data1, x);
  rec.record(T::Y, data2, x);
  rec.record(T::Z, {10*x, 20*x}, x);
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
  int num_rec_rounds  = 2;
  int num_rec_threads = 2;
  int num_ctx_threads = 1;
  std::string addr = "inproc://recorder";

  // ----------------------------------------------------------------------
  po::options_description opts("Options", 80, 120);
  opts.add_options()
      ("help,h", "Show help")
      ("verbose,v", "Be verbose")
      ("rounds,r",
       po::value<int>(&num_rec_rounds)->default_value(num_rec_rounds),
       "Number of recording rounds")
      ("threads,t",
       po::value<int>(&num_rec_threads)->default_value(num_rec_threads),
       "Number of recording threads")
      ("context_io",
       po::value<int>(&num_ctx_threads)->default_value(num_ctx_threads),
       "Number of ZMQ context io threads")
      ("address,a",
       po::value<std::string>(&addr)->default_value(addr),
       "Socket address");

  po::variables_map vm;
  po::store(po::parse_command_line(ac, av, opts), vm);
  po::notify(vm);

  if (vm.count("help")) {
    opts.print(std::cout);
    std::exit(0);
  }
  // ----------------------------------------------------------------------

  zmq::context_t ctx(num_ctx_threads);

  RecorderBase::setContext(&ctx);
  RecorderBase::setAddress(addr);

  printf("Item size: %lu\n", sizeof(Item));

  RecorderHDF5 backend;
  backend.start(vm.count("verbose"));

  int const num_recorder_per_thread = 4;
  int const num_messages_per_recorder = 8;

  // Each item recording generates two messages per round except for the
  // first, which only creates a single message.
  auto num_messages = num_rec_threads *
                      num_recorder_per_thread *
                      num_messages_per_recorder *
                      (2 * num_rec_rounds - 1);

  printf("Running %d threads %d rounds\n"
         "  %d recorders per thread\n"
         "  %d items per recorder\n"
         "Total %d (%ldMiB)\n",
         num_rec_threads,
         num_rec_rounds,
         num_recorder_per_thread,
         num_messages_per_recorder,
         num_messages,
         (num_messages * sizeof(Item))/(1024*1024));

  std::vector<std::thread> recorders;
  for (int i = 0; i < num_rec_threads; ++i) {
    recorders.emplace_back(std::thread(&funcProducer, i+1, num_rec_rounds));
  }

  for (auto& th : recorders) {
    if (th.joinable())
      th.join();
  }

  std::this_thread::sleep_for(msec(1));

  backend.stop();

  RecorderBase::shutDown();

  return 0;
}
