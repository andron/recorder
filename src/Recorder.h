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

#pragma once

#include <cassert>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "zmq.hpp"

class Recorder {
 public:
  // -------------------------------------------------------------------------
  Recorder() = delete;
  Recorder(Recorder const&) = delete;
  explicit Recorder(uint64_t id);
  Recorder(uint64_t id, std::string const address);
  ~Recorder();

  /**
     Setup parameter with key (name) and unit for recording. The unit is
     a string which must be parsed at the receiving side. Calling setup
     multiple times with the same key value will have no effect, once
     setup the key and unit will be locked.
  */
  Recorder& setup(std::string const& key, std::string const& unit);

  /**
     Record parameter with key, previously setup using setup(). The
     value need not have the same type in each call but there will be a
     difference between 1 (integer) and 1.0 (float) causing a new
     recording event to occur.

     Available specializations: char, int64_t, uint64_t, double
  */
  template<typename T>
  Recorder& record(std::string const& key, T value);

  /**
     Set class context to use for ZeroMQ communication.
  */
  static void setContext(zmq::context_t* ctx);

  /**
     Set class socket address for ZeroMQ communication.
  */
  static void setAddress(std::string address);

  struct Item {
    enum class Type : std::int32_t { INIT, OTHER, CHAR, INT, UINT, FLOAT, STR };
    Type type;
    int64_t time;
    union Data {
      char     c;
      char     s[sizeof(int64_t)];
      int64_t  i;
      uint64_t u;
      double   d;
    } data;
    char name[8];
    char unit[8];

    Item();
    Item(std::string const& name, std::string const& unit);
    std::string&& toString() const;
  };

 private:
  // ---------------------------------------------------------------------------
  static zmq::context_t* socket_context;
  static std::string     socket_address;
  static thread_local std::vector<Item> send_buffer;

  uint64_t _identifier;
  std::string _socket_address;
  std::unordered_map<std::string, Item> _storage;
  std::unique_ptr<zmq::socket_t> _socket;

  void send(Item const& item);
};
