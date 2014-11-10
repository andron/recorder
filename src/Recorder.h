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

#include <array>
#include <ctime>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>

#include "RecorderCommon.h"

template<typename K>
class Recorder : public RecorderCommon {
 public:
  Recorder(Recorder const&) = delete;
  Recorder& operator= (Recorder const&) = delete;

  typedef RecorderCommon Super;

  Recorder(int32_t id, std::string name)
      : Super(id, name) {
    Super::setupRecorder(items_.max_size());
  }

  ~Recorder() {
  }

  // Setup parameter with key (name) and unit for recording. The unit is
  // a string which must be parsed at the receiving side. Calling setup
  // multiple times with the same key value will have no effect, once
  // setup the key and unit will be locked.
  //
  // See the implementation for available instantiations of updateItem.
  Recorder& setup(K const KEY,
                  std::string const& name,
                  std::string const& unit,
                  std::string const& desc = "N/A") {
    auto const key = static_cast<decltype(Item::key)>(KEY);
    items_[key] = Item(key);
    Super::setup(ItemInit(key, name, unit, desc));
    return *this;
  }

  // Record parameter with key, previously setup using setup(). The
  // value need not have the same type in each call but there will be a
  // difference between 1 (integer) and 1.0 (float) causing a new
  // recording event to occur.
  template<typename V>
  Recorder& record(K const key, V const value) {
    auto const time = std::time(nullptr);
    auto& item = items_[static_cast<size_t>(key)];
    if (item.type == ItemType::INIT) {
      updateItem(&item, time, value);
      Super::record(item);
    } else if (std::memcmp(&(item.data), &value, sizeof(value))) {
      item.time = time;
      Super::record(item);
      updateItem(&item, time, value);
      Super::record(item);
    } else {
      // Ignore unchanged value
    }
    return *this;
  }

 private:
  // Local storage for recorder. Each recorded item is appended to the
  // array and when it is full it is copied to a zeromq message buffer
  // for transport.
  std::array<Item, static_cast<size_t>(K::Count)> items_;
};
