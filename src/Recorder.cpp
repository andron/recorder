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

#include <cassert>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <type_traits>
#include <unordered_map>

//  Utilities
//  ------------------------------------------------------------
namespace {

void Error(char const* msg) { std::fprintf(stderr, "%s\n", msg); }

//  Create a item from name, value and time paramters.
//  Specialization for char const*
Recorder::Item
createItem(Recorder::Item const& clone,
           int64_t const time,
           char const* value) {
  Recorder::Item item;
  std::memcpy(&item, &clone, sizeof(item));
  item.time = time;
  item.type = Recorder::Item::Type::STR;
  strncpy(&item.data.s[0], &value[0], sizeof(item.data.s));
  return item;
}

template<typename T> Recorder::Item
createItem(Recorder::Item const& clone,
           int64_t const time,
           T const value) {
  Recorder::Item item;
  std::memcpy(&item, &clone, sizeof(item));
  item.time = time;
  if (std::is_same<char, T>::value) {
    item.type   = Recorder::Item::Type::CHAR;
    item.data.c = value;
  } else if (std::is_integral<T>::value) {
    if (std::is_signed<T>::value) {
      item.type   = Recorder::Item::Type::INT;
      item.data.i = value;
    } else if (std::is_unsigned<T>::value) {
      item.type   = Recorder::Item::Type::UINT;
      item.data.u = value;
    } else {
      static_assert(
          std::is_signed<T>::value || std::is_unsigned<T>::value,
          "Unknown signedness for integral type");
    }
  } else if (std::is_floating_point<T>::value) {
    item.type   = Recorder::Item::Type::FLOAT;
    item.data.d = value;
  } else {
    static_assert(
        std::is_integral<T>::value || std::is_floating_point<T>::value,
        "Value type must be char, integral or floating point");
  }
  return item;
}

} //  namespace


template Recorder&
Recorder::record<char>(std::string const& key, char value);

template Recorder&
Recorder::record<int32_t>(std::string const& key, int32_t value);

template Recorder&
Recorder::record<int64_t>(std::string const& key, int64_t value);

template Recorder&
Recorder::record<uint32_t>(std::string const& key, uint32_t value);

template Recorder&
Recorder::record<uint64_t>(std::string const& key, uint64_t value);

template Recorder&
Recorder::record<double>(std::string const& key, double value);


Recorder::Item::Item()
    : type(Type::INIT)
    , time(-1) {
  memset(&data, 0, sizeof(data));
}


std::string
Recorder::Item::toString() const {
  char buffer[128];
  std::string name(this->name);
  std::string unit(this->unit);
  unit = "[" + unit + "]";
  snprintf(buffer, sizeof(buffer), "%ld %10s = ", this->time, name.c_str());
  switch (this->type) {
    case Type::CHAR:
      sprintf(buffer, "%c", data.c);
      break;
    case Type::INT:
      sprintf(buffer, "%ld", data.i);
      break;
    case Type::UINT:
      sprintf(buffer, "%lu", data.u);
      break;
    case Type::FLOAT:
      sprintf(buffer, "%f", data.d);
      break;
    case Type::STR:
      {
        std::string str(data.s, sizeof(data.s));
        sprintf(buffer, "%s", str.c_str());
      }
      break;
    default:
      break;
  }
  snprintf(buffer, 2 + sizeof(this->unit), "%s\n", unit.c_str());
  return std::string(buffer);
}


Recorder::Item::Item(std::string const& n, std::string const& u)
    : Item() {
  if (n.length() > sizeof(name)) {
    Error("Maximum size of parameter name 8 chars, truncating");
  } else if (u.length() > sizeof(unit)) {
    Error("Maximum size of parameter unit 8 chars, truncating");
  }
  std::strncpy(name, n.c_str(), sizeof(name));
  std::strncpy(unit, u.c_str(), sizeof(unit));
}


Recorder::Recorder(uint64_t id, std::string const address)
    : send_buffer_index_(0)
    , socket_address_(address)
    , identifier_(id) {
  items_.reserve(256);

  bool error = false;

  if (Recorder::socket_context == nullptr) {
    Error("Recorder: setContext() must be called before instantiation");
    error = true;
  }

  if (socket_address_.empty()) {
    Error("Recorder: Socket address empty, setAddress() must be called");
    Error("          before first instantiation or provide local address");
    Error("          using ctor Recorder(uint64_t, std::string)");
    error = true;
  }

  if (error) {
    std::exit(1);
  }

  constexpr int linger = 3000;
  constexpr int sendtimeout = 2;

  socket_.reset(new zmq::socket_t(*Recorder::socket_context, ZMQ_PUSH));
  socket_->setsockopt(ZMQ_LINGER, &linger, sizeof(linger));
  socket_->setsockopt(ZMQ_SNDTIMEO, &sendtimeout, sizeof(sendtimeout));

  zmqutils::connect(socket_.get(), socket_address_);
}


Recorder::Recorder(uint64_t id)
    : Recorder(id, Recorder::socket_address) {
}


Recorder::~Recorder() {
  flushSendBuffer();
  if (socket_) {
    socket_->close();
  }
}


Recorder&
Recorder::setup(std::string const& key, std::string const& unit) {
  auto it = items_.find(key);
  if (it == items_.end()) {
    Item item(key, unit);
    items_.emplace(key, item);
  } else {
    // Ignore already setup parameters
  }
  return (*this);
}


template<> Recorder&
Recorder::record<std::string const&>(std::string const& key,
                                     std::string const& str) {
  // Ignoring strings for now, converting to byte
  record(key, str.c_str()[0]);
  return *this;
}

template<> Recorder&
Recorder::record<char const*>(std::string const& key,
                              char const* str) {
  // Ignoring strings for now, converting to byte
  record(key, str[0]);
  return *this;
}

template<typename T> Recorder&
Recorder::record(std::string const& key, T const value) {
  auto const time = std::time(nullptr);
  auto const it = items_.find(key);
  if (it == items_.end()) {
    assert(0 && "Must use setup() prior to record()");
  } else if (it->second.type == Item::Type::INIT) {
    auto const item = createItem(it->second, time, value);
    items_[key] = item;
    send(item);
  } else if (std::memcmp(&(it->second.data), &value, sizeof(value))) {
    auto const item = createItem(it->second, time, value);
    it->second.time = time;
    send(it->second);
    items_[key] = item;
    send(item);
  } else {
    // Ignore unchanged value
  }
  return *this;
}


void
Recorder::send(Item const& item) {
  send_buffer_[send_buffer_index_++] = item;
  if (send_buffer_index_ == send_buffer_.max_size()) {
    flushSendBuffer();
  }
}


void
Recorder::flushSendBuffer() {
  constexpr auto item_size = sizeof(decltype(send_buffer_)::value_type);
  if (send_buffer_index_ > 0) {
    socket_->send(send_buffer_.data(), send_buffer_index_ * item_size);
    send_buffer_index_ = 0;
  }
}


void
Recorder::setContext(zmq::context_t* ctx) {
  socket_context = ctx;
}

void
Recorder::setAddress(std::string addr) {
  socket_address = addr;
}

zmq::context_t* Recorder::socket_context = nullptr;
std::string     Recorder::socket_address = "";
