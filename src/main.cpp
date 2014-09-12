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

#include "proto/test.pb.h"

#include <zmq.hpp>

#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdlib>
#include <functional>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <sstream>
#include <string>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace {
typedef std::chrono::milliseconds msec;
}

int
main(int, char**) {
  zmq::context_t ctx(1);

  char* buf = (char*)malloc(32*1e6);

  for (int i = 0; i < 3e5; ++i) {
    Position p;
    std::string str;
    p.set_time(i * 0.1);
    p.set_x   (i * 1.0);
    p.set_y   (i * 2.0);
    p.set_z   (i * 4.0);
    p.SerializeToString(&str);
    memcpy(buf + i * p.ByteSize(), str.c_str(), str.length());
  }
  
  Recorder::setContext(&ctx);
  Recorder::setAddress("inproc://recorder");

  char const id[] = "t301";
  char const* foppa = "foppa";
  std::string zzz   = "zzz";

  Recorder rec1(100);
  Recorder rec2(201);
  rec1.setup("foo", "m");
  rec2.setup("foo", "m/s");
  rec2.setup("fiii", "s");
  rec1.record("foo", 1.1);
  rec1.record("foo", 1.1);
  rec2.record("fiii", 1);
  rec2.record("fiii", 1);
  rec2.record("foo", 9.1);
  rec2.record("foo", 9.1);
  rec2.record("foo", id);
  rec2.record("foo", foppa);
  rec2.record("foo", foppa);
  rec2.record("foo", foppa);
  rec2.record("foo", 'c');
  rec2.record("foo", 'c');
  rec2.record("foo", 'c');
  rec1.record("foo", 1.2);
  rec1.record("foo", 1.2);
  rec1.record("foo", 1.2);
  rec2.record("foo", 9.1);
  rec2.record("foo", 9.2);
  rec2.record("foo", 9.3);
  rec2.record("foo", 9.4);
  rec2.record("foo", 9.4);
  rec2.record("foo", 9.4);
  rec2.record("foo", 9.4);
  rec1.record("foo", 1.2);
  rec2.record("foo", zzz.c_str());
  rec2.record("foo", zzz.c_str());
  rec2.record("foo", zzz.c_str());
  rec2.record("foo", zzz.c_str());
  rec2.record("foo", zzz.c_str());
  rec2.record("foo", zzz.c_str());
  rec1.record("foo", 1.2);
  rec1.record("foo", 1.2);
  rec2.record("fiii", 2);
  rec2.record("fiii", 3);
  rec2.record("fiii", 4);
  rec1.record("foo", 1.2);
  rec1.record("foo", 1.2);
  rec2.record("foo", 9.4);
  rec2.record("foo", 9.4);
  rec1.record("foo", 1.2);
  rec1.record("foo", 1.0);
  rec2.record("fiii", 3.3);
  rec1.record("foo", 1.0);
  rec1.record("foo", 1.0);
  rec1.record("foo", 1.0);
  rec1.record("foo", 1.0);
  rec1.record("foo", 1.1);

  return 0;
}
