// -*- mode:c++; indent-tabs-mode:nil; -*-

#pragma once

#include "zmqutils.h"

#include <cassert>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <type_traits>
#include <unordered_map>
#include <memory>

class Recorder : public zmqutils::Socket
{
 public:
  Recorder() = delete;
  Recorder(Recorder const&) = delete;

  explicit Recorder(uint64_t object_identifier)
      : _object_identifier(object_identifier) {
    storage.reserve(128);
    if (zmqutils::Socket::socket_context != nullptr) {
      socket.reset(new zmq::socket_t(*zmqutils::Socket::socket_context, ZMQ_PUSH));
      socket->connect(zmqutils::Socket::socket_address.c_str());
    }
  }

  ~Recorder() {
    if (socket) {
      socket->close();
    }
  }

  Recorder& setup(std::string const& key, std::string const& unit) {
    auto it = storage.find(key);
    if (it == storage.end()) {
      Item item(key, unit);
      storage.emplace(key, item);
    } else {
      // Ignore already setup parameters
    }
    return (*this);
  }

  template<typename T>
  Recorder& record(std::string const& key, T const& value) {
    auto const time = std::clock();
    auto it = storage.find(key);
    if (it == storage.end()) {
      assert(0 && "Must use setup() prior to record()");
    } else if (it->second.type == Item::INIT) {
      auto const item = createItem(it->second, time, value);
      storage[key] = item;
      send(item);
    } else if (memcmp(&value, &(it->second.data), sizeof(value)) != 0) {
      auto const item = createItem(it->second, time, value);
      it->second.time = time;
      send(it->second);
      storage[key] = item;
      send(item);
    } else {
      // Ignore unchanged value
    }
    return (*this);
  }

 private:

  struct Item {
    enum Type { INIT, OTHER, INT, UINT, FLOAT } type;
    int64_t time;
    union Data {
      int64_t  i;
      uint64_t u;
      double   d;
    } data;
    char name[8];
    char unit[8];

    Item() : type(INIT), time(-1) {}

    Item(std::string const& n, std::string const& u) : Item() {
      std::strncpy(name, n.c_str(), sizeof(name));
      std::strncpy(unit, u.c_str(), sizeof(unit));
    }
  };

  uint64_t _object_identifier;
  std::unordered_map<std::string, Item> storage;
  std::unique_ptr<zmq::socket_t> _socket;

  /**
   * Create a item from name, value and time paramters.
   */
  template<typename T>
  Item createItem(Item const& clone, int64_t time, T const& value) const {
    Item item;
    std::memcpy(&item, &clone, sizeof(item));
    item.time = time;
    if (std::is_integral<T>::value) {
      if (std::is_signed<T>::value) {
        item.type   = Item::INT;
        item.data.i = value;
      } else if (std::is_unsigned<T>::value) {
        item.type   = Item::UINT;
        item.data.u = value;
      } else {
        static_assert(
            std::is_signed<T>::value || std::is_unsigned<T>::value,
            "Unknown signedness for integral type");
      }
    } else if (std::is_floating_point<T>::value) {
      item.type   = Item::FLOAT;
      item.data.d = value;
    } else {
      static_assert(
          std::is_integral<T>::value || std::is_floating_point<T>::value,
          "Value type must be integral or floating point");
    }
    return item;
  }

  void send(Item const& item) const {
    std::string name(item.name);
    std::string unit(item.unit);
    std::cout << "rec:" << _object_identifier << " -- "
              << item.time << ":" << name << ":[" << unit << "] = ";
    switch (item.type) {
      case Item::INT:
        std::cout << item.data.i;
        break;
      case Item::UINT:
        std::cout << item.data.u;
        break;
      case Item::FLOAT:
        std::cout << item.data.d;
        break;
      default:
        //assert(0 && "Unknown type " && item.type);
        break;
    }
    std::cout << std::endl << std::flush;
  }

};
