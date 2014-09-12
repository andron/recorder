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

#include <iostream>

#include "zmqutils.h"


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
    : type(INIT), time(-1) {
}

Recorder::Item::Item(std::string const& n, std::string const& u)
    : Item() {
  std::strncpy(name, n.c_str(), sizeof(name));
  std::strncpy(unit, u.c_str(), sizeof(unit));
}


void Error(char const* msg) { std::fprintf(stderr, "%s\n", msg); }

Recorder::Recorder(uint64_t id) : _identifier(id) {
  _storage.reserve(128);

  bool error = false;
  if (Recorder::socket_context == nullptr) {
    Error("Recorder: setContext() must be called before instantiation");
    error = true;
  }
  if (Recorder::socket_address.empty()) {
    Error("Recorder: setAddress() must be called before instantiation");
    error = true;
  }

  if (error) {
    std::exit(1);
  }

  _socket.reset(new zmq::socket_t(*Recorder::socket_context, ZMQ_PUSH));
  zmqutils::connect(_socket.get(), Recorder::socket_address);
}

Recorder::~Recorder() {
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
    item.type   = Recorder::Item::CHAR;
    item.data.c = value;
  } else if (std::is_integral<T>::value) {
    if (std::is_signed<T>::value) {
      item.type   = Recorder::Item::INT;
      item.data.i = value;
    } else if (std::is_unsigned<T>::value) {
      item.type   = Recorder::Item::UINT;
      item.data.u = value;
    } else {
      static_assert(
          std::is_signed<T>::value || std::is_unsigned<T>::value,
          "Unknown signedness for integral type");
    }
  } else if (std::is_floating_point<T>::value) {
    item.type   = Recorder::Item::FLOAT;
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
//   item.type = Recorder::Item::STR;
//   strncpy(&item.data.s[0], &value[0], sizeof(item.data.s));
//   return item;
//}
// ---------------------------------------------------------------------------

template<typename T>
Recorder& Recorder::record(std::string const& key, T const value) {
  auto const time = std::clock();
  auto it = _storage.find(key);
  if (it == _storage.end()) {
    assert(0 && "Must use setup() prior to record()");
  } else if (it->second.type == Item::INIT) {
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
Recorder::send(Item const& item) const {
  std::string name(item.name);
  std::string unit(item.unit);
  unit = "[" + unit + "]";
  printf("[%lu] %ld %10s:%-6s = ",
         _identifier, item.time, name.c_str(), unit.c_str());
  switch (item.type) {
    case Item::CHAR:
      printf("%c\n", item.data.c);
      break;
    case Item::INT:
      printf("%ld\n", item.data.i);
      break;
    case Item::UINT:
      printf("%lu\n", item.data.u);
      break;
    case Item::FLOAT:
      printf("%f\n", item.data.d);
      break;
    case Item::STR:
      {
        std::string str(item.data.s, sizeof(item.data.s));
        printf("%s\n", str.c_str());
      }
      break;
    default:
      break;
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
