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

#include "RecorderBase.h"

#include <array>
#include <ctime>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>

template<typename K>
class Recorder : public RecorderBase {
 public:
  Recorder(Recorder const&) = delete;
  Recorder& operator= (Recorder const&) = delete;

  Recorder(std::string name, int32_t external_id = 0)
      : RecorderBase(name, external_id) {
    RecorderBase::setupRecorder(items_.max_size());
  }

  ~Recorder() {
  }

  // Setup parameter with key (name) and unit for recording. The unit is
  // a string which must be parsed at the receiving side. Calling setup
  // multiple times with the same key value will have no effect, once
  // setup the key and unit will be locked.
  //
  // See the implementation for available instantiations of updateItem.
  void setup(K const enumkey,
             std::string const& name,
             std::string const& unit,
             std::string const& desc = "N/A") {
    auto const key = static_cast<decltype(Item::key)>(enumkey);
    items_[key] = Item(key);
    RecorderBase::setup(ItemInit(key, name, unit, desc));
  }

  // Record parameter with key, previously setup using setup(). The
  // value need not have the same type in each call but there will be a
  // difference between 1 (integer) and 1.0 (float) causing a new
  // recording event to occur.
  template<typename V>
  void record(K const enumkey, V const value) {
    // TODO() time value must be provided.
    auto const time = std::time(nullptr);
    auto& item = items_[static_cast<decltype(Item::key)>(enumkey)];
    if (item.type == ItemType::INIT) {
      updateItem(&item, time, value);
      RecorderBase::record(item);
    } else if (std::memcmp(&(item.data), &value, sizeof(value))) {
      // First send old value at current time
      item.time = time;
      RecorderBase::record(item);
      // Then update item and send again at current time
      updateItem(&item, time, value);
      RecorderBase::record(item);
    } else {
      // Ignore unchanged value
    }
  }

 private:
  // Local storage for recorder. Each recorded item is appended to the
  // array and when it is full it is copied to a zeromq message buffer
  // for transport.
  std::array<Item, static_cast<size_t>(K::Count)> items_;
};
