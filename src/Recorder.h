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
#include <cstdlib>
#include <ctime>
#include <memory>
#include <string>


namespace {

template<typename T, typename U, size_t N>
void huffcopy(T (&dst)[N], U const (&src)[N]) {
  switch (N) {
    case 3: dst[2] = src[2];
    case 2: dst[1] = src[1];
    case 1: dst[0] = src[0];
  };
}

template<typename V, size_t N>
void updateData(Item* item, V const (&v)[N]) {
  if (std::is_same<V, int32_t>::value) {
    int64_t dst[N];
    huffcopy(dst, v);
    std::memcpy(&item->data, &dst, sizeof(dst));
  } else if (std::is_same<V, uint32_t>::value) {
    uint64_t dst[N];
    huffcopy(dst, v);
    std::memcpy(&item->data, &dst, sizeof(dst));
  } else if (std::is_same<V, float>::value) {
    double dst[N];
    huffcopy(dst, v);
    std::memcpy(&item->data, &dst, sizeof(dst));
  } else {
    std::memcpy(&item->data, &v, sizeof(v));
  }
}

}  // namespace


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
  void setup(K const enumkey,
             std::string const& name,
             std::string const& unit,
             std::string const& desc = "N/A") {
    auto const key = static_cast<decltype(Item::key)>(enumkey);
    items_[key] = Item(key);
    RecorderBase::setupItem(InitItem(recorder_id_, key, name, unit, desc));
  }

  // Record parameter with key, previously setup using setup(). The
  // value need not have the same type in each call but there will be a
  // difference between 1 (integer) and 1.0 (float) causing a new
  // recording event to occur.
  template<typename V, size_t N>
  void record(K const enumkey, V const (&value)[N], uint64_t time = 1) {
    static_assert(N <= 3, "Maximum array size is 3");
    auto& item = items_[static_cast<size_t>(enumkey)];

    if (item.info == ItemType::NOTSETUP) {
      printf("Warning: Not setup item enum %d[%lu] \"%s\"\n",
             enumkey, N, recorder_name_.c_str());
    } else if (item.info == ItemType::INIT) {
      // At this point we know the data type. Will only happen once and
      // from now on the receiver will have to stick with this data type
      // for storage.
      setDataType<V, N>(&item);
      // Set time
      item.time = time;
      // Set recorder id
      item.recorder_id = recorder_id_;
      // Set data
      updateData(&item, value);
      // Record
      RecorderBase::record(item);
    } else if (std::memcmp(&(item.data), &value, sizeof(value))) {
      // First record old value at current time
      item.time = time;
      RecorderBase::record(item);
      // Then update value and record again at current time
      updateData(&item, value);
      RecorderBase::record(item);
    } else {
      // Ignore unchanged value
    }
  }

  // For single values record calls.
  template<typename V>
  void record(K const enumkey, V const value) {
    record(enumkey, {value});
  }

 private:
  // Local storage for recorder. Each recorded item is appended to the
  // array and when it is full it is copied to a zeromq message buffer
  // for transport.
  std::array<Item, static_cast<size_t>(K::Count)> items_;
};
