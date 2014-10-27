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

#include <zmq.hpp>

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <type_traits>

namespace {
void Error(char const* msg) { std::fprintf(stderr, "%s\n", msg); }
}


//  Clone functions for aritmetic types
//  -------------------------------------------------------------------------
template Item
cloneItem<char>(Item const&, int64_t const, char const);

template Item
cloneItem<int32_t>(Item const&, int64_t const, int32_t const);

template Item
cloneItem<int64_t>(Item const&, int64_t const, int64_t const);

template Item
cloneItem<uint32_t>(Item const&, int64_t const, uint32_t const);

template Item
cloneItem<uint64_t>(Item const&, int64_t const, uint64_t const);

template Item
cloneItem<double>(Item const&, int64_t const, double const);


template<> Item
cloneItem<char const*>(Item const& clone,
                       int64_t const time,
                       char const* value) {
  Item item;
  std::memcpy(&item, &clone, sizeof(item));
  item.time = time;
  item.type = Item::Type::STR;
  std::strncpy(&item.data.s[0], &value[0], sizeof(item.data.s));
  return item;
}

template<typename V> Item
cloneItem(Item const& clone,
          int64_t const time,
          V const value) {
  Item item;
  std::memcpy(&item, &clone, sizeof(item));
  item.time = time;
  if (std::is_same<char, V>::value) {
    item.type   = Item::Type::CHAR;
    item.data.c = value;
  } else if (std::is_integral<V>::value) {
    if (std::is_signed<V>::value) {
      item.type   = Item::Type::INT;
      item.data.i = value;
    } else if (std::is_unsigned<V>::value) {
      item.type   = Item::Type::UINT;
      item.data.u = value;
    } else {
      static_assert(
          std::is_signed<V>::value || std::is_unsigned<V>::value,
          "Unknown signedness for integral type (?)");
    }
  } else if (std::is_floating_point<V>::value) {
    item.type   = Item::Type::FLOAT;
    item.data.d = value;
  } else {
    item.type   = Item::Type::OTHER;
    std::memcpy(&item.data,
                &value,
                std::min(sizeof(value), sizeof(item.data)));
  }
  return item;
}
//  -------------------------------------------------------------------------


//!  Set class context to use for ZeroMQ communication.
void RecorderCommon::setContext(zmq::context_t* context) {
  socket_context = context;
}

//!  Set class socket address for ZeroMQ communication.
void RecorderCommon::setAddress(std::string address) {
  printf("Address: %s\n", address.c_str());
  socket_address = address;
}

zmq::context_t* RecorderCommon::socket_context = nullptr;
std::string     RecorderCommon::socket_address = "";

thread_local RecorderCommon::SendBuffer
RecorderCommon::send_buffer;

thread_local RecorderCommon::SendBuffer::size_type
RecorderCommon::send_buffer_index = 0;

RecorderCommon::RecorderCommon() {
  bool error = false;

  if (RecorderCommon::socket_context == nullptr) {
    Error("setContext() must be called before instantiation");
    error = true;
  }

  if (RecorderCommon::socket_address.empty()) {
    Error("Socket address empty, setAddress() must be called");
    Error("before first instantiation");
    error = true;
  }

  if (error) {
    std::exit(1);
  }

  int constexpr linger = 3000;
  int constexpr sendtimeout = 2;

  socket_.reset(new zmq::socket_t(*RecorderCommon::socket_context, ZMQ_PUSH));
  socket_->setsockopt(ZMQ_LINGER, &linger, sizeof(linger));
  socket_->setsockopt(ZMQ_SNDTIMEO, &sendtimeout, sizeof(sendtimeout));
  zmqutils::connect(socket_.get(), RecorderCommon::socket_address);
}

RecorderCommon::~RecorderCommon() {
  flushSendBuffer();
  socket_->close();
  socket_.reset();
}


std::string
RecorderCommon::getAddress() const {
  return RecorderCommon::socket_address;
}


void
RecorderCommon::flushSendBuffer() {
  constexpr auto item_size = sizeof(decltype(send_buffer)::value_type);
  if (send_buffer_index > 0) {
    socket_->send(send_buffer.data(), send_buffer_index * item_size);
    send_buffer_index = 0;
  }
}

void
RecorderCommon::record(Item const& item) {
  send_buffer[send_buffer_index++] = item;
  if (send_buffer_index == send_buffer.max_size()) {
    flushSendBuffer();
  }
}
