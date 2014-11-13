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

#include "RecorderBase.h"

#include <algorithm>
#include <atomic>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <type_traits>

#include <zmq.hpp>

#include "zmqutils.h"


namespace {
void Error(char const* msg) { std::fprintf(stderr, "%s\n", msg); }
std::atomic<int16_t> g_recorder_id = ATOMIC_VAR_INIT(0);
}


// Clone functions for Item
// ----------------------------------------------------------------------------
// Instantiations
template void
updateItem<char>(Item*, int32_t const, char const);

template void
updateItem<int32_t>(Item*, int32_t const, int32_t const);

template void
updateItem<int64_t>(Item*, int32_t const, int64_t const);

template void
updateItem<uint32_t>(Item*, int32_t const, uint32_t const);

template void
updateItem<uint64_t>(Item*, int32_t const, uint64_t const);

template void
updateItem<double>(Item*, int32_t const, double const);

// Specialization
template<> void
updateItem<char const*>(Item* item, int32_t const time, char const* value) {
  item->time = time;
  item->type = ItemType::STR;
  std::strncpy(&item->data.s[0], &value[0], sizeof(item->data.s));
}

// Definition
template<typename V> void
updateItem(Item* item, int32_t const time, V const value) {
  item->time = time;
  if (std::is_same<char, V>::value) {
    item->type   = ItemType::CHAR;
    item->data.c = value;
  } else if (std::is_integral<V>::value) {
    if (std::is_signed<V>::value) {
      item->type   = ItemType::INT;
      item->data.i = value;
    } else if (std::is_unsigned<V>::value) {
      item->type   = ItemType::UINT;
      item->data.u = value;
    } else {
      static_assert(
          std::is_signed<V>::value || std::is_unsigned<V>::value,
          "Unknown signedness for integral type (?)");
    }
  } else if (std::is_floating_point<V>::value) {
    item->type   = ItemType::FLOAT;
    item->data.d = value;
  } else {
    item->type   = ItemType::OTHER;
    std::memcpy(&item->data,
                &value,
                std::min(sizeof(value), sizeof(item->data)));
  }
}
// ----------------------------------------------------------------------------


// Static definitions for RecorderBase
// ----------------------------------------------------------------------------
void
RecorderBase::setContext(zmq::context_t* context) {
  socket_context = context;
}

void
RecorderBase::setAddress(std::string address) {
  printf("Address: %s\n", address.c_str());
  socket_address = address;
}

std::string
RecorderBase::getAddress() {
  return RecorderBase::socket_address;
}

zmq::context_t* RecorderBase::socket_context = nullptr;
std::string     RecorderBase::socket_address = "";

thread_local
RecorderBase::SendBuffer RecorderBase::send_buffer;

thread_local
RecorderBase::SendBuffer::size_type RecorderBase::send_buffer_index = 0;
// ----------------------------------------------------------------------------


RecorderBase::RecorderBase(std::string name, int32_t id)
    : external_id_(id)
    , recorder_id_(g_recorder_id.fetch_add(1))
    , recorder_name_(name) {
  bool error = false;

  if (RecorderBase::socket_context == nullptr) {
    Error("setContext() must be called before instantiation");
    error = true;
  }

  if (RecorderBase::socket_address.empty()) {
    Error("Socket address empty, setAddress() must be called");
    Error("before first instantiation");
    error = true;
  }

  if (error) {
    std::exit(1);
  }

  int constexpr linger = 3000;
  int constexpr sendtimeout = 2;

  socket_.reset(new zmq::socket_t(*RecorderBase::socket_context, ZMQ_PUSH));
  socket_->setsockopt(ZMQ_LINGER, &linger, sizeof(linger));
  socket_->setsockopt(ZMQ_SNDTIMEO, &sendtimeout, sizeof(sendtimeout));
  zmqutils::connect(socket_.get(), RecorderBase::socket_address);
}

RecorderBase::~RecorderBase() {
  flushSendBuffer();
  socket_->close();
}

void
RecorderBase::flushSendBuffer() {
  constexpr auto item_size = sizeof(decltype(send_buffer)::value_type);
  if (send_buffer_index > 0) {
    PayloadFrame const frame(recorder_id_, PayloadType::DATA);
    socket_->send(&frame, sizeof(frame), ZMQ_SNDMORE);
    socket_->send(send_buffer.data(), send_buffer_index * item_size);
    send_buffer_index = 0;
  }
}

void
RecorderBase::setupRecorder(int32_t num_items) {
  PayloadFrame const frame(0, PayloadType::INIT_RECORDER);
  InitRecorder const init_rec(
      external_id_, recorder_id_, num_items, recorder_name_);
  socket_->send(&frame, sizeof(frame), ZMQ_SNDMORE);
  socket_->send(&init_rec, sizeof(init_rec));
}

void
RecorderBase::setupItem(InitItem const& init_item) {
  PayloadFrame const frame(recorder_id_, PayloadType::INIT_ITEM);
  socket_->send(&frame, sizeof(frame), ZMQ_SNDMORE);
  socket_->send(&init_item, sizeof(init_item));
}

void
RecorderBase::record(Item const& item) {
  send_buffer[send_buffer_index++] = item;
  if (send_buffer_index == send_buffer.max_size()) {
    flushSendBuffer();
  }
}
