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

#include <cassert>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <type_traits>
#include <unordered_map>

#include "zmqutils.h"

void Error(char const* msg) { std::fprintf(stderr, "%s\n", msg); }

// Template instantiation
template<> Recorder&
Recorder::record<char const*>(std::string const& key, char const* value) {
  record(key, *value);
  return (*this);
}

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
    : type(Type::INIT), time(-1) {
}

std::string&&
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
  snprintf(buffer, 2+sizeof(this->unit), "%s\n", unit.c_str());
  return std::move(std::string(buffer));
}

Recorder::Item::Item(std::string const& n, std::string const& u)
    : Item() {
  if (n.length() > sizeof(name)) {
    Error("Maximum size of parameter name 8 chars");
    std::exit(1);
  } else if (u.length() > sizeof(unit)) {
    Error("Maximum size of parameter unit 8 chars");
    std::exit(1);
  }
  std::strncpy(name, n.c_str(), sizeof(name));
  std::strncpy(unit, u.c_str(), sizeof(unit));
}

Recorder::Recorder(uint64_t id, std::string const address)
    : _identifier(id)
    , _socket_address(address)
{
  _storage.reserve(256);

  bool error = false;
  if (Recorder::socket_context == nullptr) {
    Error("Recorder: setContext() must be called before instantiation");
    error = true;
  }
  if (_socket_address.empty()) {
    Error("Recorder: Socket address empty, setAddress() must be called");
    Error("          before first instantiation or provide local address");
    Error("          using ctor Recorder(uint64_t, std::string)");
    error = true;
  }

  if (error) {
    std::exit(1);
  }

  constexpr int linger = 3000;

  _socket.reset(new zmq::socket_t(*Recorder::socket_context, ZMQ_PUSH));
  _socket->setsockopt(ZMQ_LINGER, &linger, sizeof(linger));

  zmqutils::connect(_socket.get(), _socket_address);
}

Recorder::Recorder(uint64_t id)
    : Recorder(id, Recorder::socket_address) {
}

Recorder::~Recorder() {
  flushSendBuffer();
  if (_socket) {
    _socket->close();
  }
}

Recorder&
Recorder::setup(std::string const& key, std::string const& unit) {
  auto it = _storage.find(key);
  if (it == _storage.end()) {
    Item item(key, unit);
    _storage.emplace(key, item);
  } else {
    // Ignore already setup parameters
  }
  return (*this);
}


//!! Create a item from name, value and time paramters.
// ---------------------------------------------------------------------------
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
        "Value type must be integral or floating point");
  }
  return item;
}

// Specialization for char const*
// template<> Recorder::Item
// createItem(Recorder::Item const& clone,
//            int64_t const time,
//            char value[]) {
//   Recorder::Item item;
//   std::memcpy(&item, &clone, sizeof(item));
//   item.time = time;
//   item.type = Type::STR;
//   strncpy(&item.data.s[0], &value[0], sizeof(item.data.s));
//   return item;
//}
// ---------------------------------------------------------------------------

template<typename T>
Recorder& Recorder::record(std::string const& key, T const value) {
  auto const time = std::time(nullptr);
  auto it = _storage.find(key);
  if (it == _storage.end()) {
    assert(0 && "Must use setup() prior to record()");
  } else if (it->second.type == Item::Type::INIT) {
    auto const item = createItem(it->second, time, value);
    _storage[key] = item;
    send(item);
  } else if (memcmp(&value, &(it->second.data), sizeof(value)) != 0) {
    auto const item = createItem(it->second, time, value);
    it->second.time = time;
    send(it->second);
    _storage[key] = item;
    send(item);
  } else {
    // Ignore unchanged value
  }
  return (*this);
}


void
Recorder::send(Item const& item) {
  _send_buffer[_send_buffer_index++] = item;
  if (_send_buffer_index == _send_buffer.max_size()) {
    flushSendBuffer();
  }
}


void
Recorder::flushSendBuffer() {
  constexpr auto item_size = sizeof(decltype(_send_buffer)::value_type);
  if (_send_buffer_index > 0) {
    _socket->send(_send_buffer.data(), _send_buffer_index * item_size);
    _send_buffer_index = 0;
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

//thread_local Recorder::SendBuffer Recorder::send_buffer;
//thread_local Recorder::SendBuffer::size_type Recorder::send_buffer_idx = 0;
