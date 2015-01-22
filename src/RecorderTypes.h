// -*- mode:c++; indent-tabs-mode:nil; -*-

/*
  Copyright (c) 2014, 2015, Anders Ronnbrant, anders.ronnbrant@gmail.com

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

#include <cstdint>
#include <cstring>
#include <string>

#define PACKED __attribute__((packed))

#define CHECK_POW2_SIZE(X)                                              \
  static_assert((((sizeof(X) << 1)-1) & sizeof(X)) == sizeof(X),        \
                "Size of " #X " shall be power of 2");

// Item being passed around on the ZeroMQ-bus.
// ----------------------------------------------------------------------------
enum class PayloadType {
  INIT_RECORDER, INIT_ITEM, DATA, };

struct PACKED InitRecorder {
  InitRecorder(int64_t ext_id,
               int16_t rec_id,
               int16_t num_items,
               std::string name)
      : external_id(ext_id)
      , recorder_id(rec_id)
      , recorder_num_items(num_items) {
    std::strncpy(recorder_name, name.c_str(), sizeof(recorder_name));
  }
  int64_t external_id;
  int16_t recorder_id;
  int16_t recorder_num_items;
  char recorder_name[52];
};

enum class ItemType : std::int8_t {
  NOTSETUP, INIT, OTHER, CHAR, INT, UINT, FLOAT, };

struct PACKED InitItem {
  InitItem(int16_t recorder_id,
           int16_t key,
           std::string const& name,
           std::string const& desc);

  int16_t recorder_id;
  int16_t key;
  char name[32];
  char desc[220];
};

struct PACKED Item {
  Item();
  explicit Item(int8_t key);

  int32_t  time;
  int16_t  key;
  ItemType type;
  int8_t   length;
  union Data {
    char     c;
    int64_t  i;
    uint64_t u;
    double   d;
    int64_t  v_i[3];
    uint64_t v_u[3];
    double   v_d[3];
    char     s[sizeof(v_i)];
  } data;
};

CHECK_POW2_SIZE(InitRecorder);
CHECK_POW2_SIZE(InitItem);
CHECK_POW2_SIZE(Item);

template<typename V, int N>
void setDataType(Item* item) {
  ItemType type;
  if (std::is_same<char, V>::value) {
    type = ItemType::CHAR;
  } else if (std::is_integral<V>::value) {
    if (std::is_signed<V>::value) {
      type = ItemType::INT;
    } else if (std::is_unsigned<V>::value) {
      type = ItemType::UINT;
    } else {
      type = ItemType::OTHER;
    }
  } else if (std::is_floating_point<V>::value) {
    type = ItemType::FLOAT;
  } else {
    type = ItemType::OTHER;
  }
  item->type = type;
  item->length = N;
}
