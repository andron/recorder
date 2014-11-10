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


namespace zmq {
class context_t;
class socket_t;
}

#define PACKED __attribute__((packed))

#define CHECK_POW2_SIZE(X)                                              \
  static_assert((((sizeof(X) << 1)-1) & sizeof(X)) == sizeof(X),        \
                "Size of " #X " shall be power of 2");

// Item being passed around on the ZeroMQ-bus.
// ----------------------------------------------------------------------------
enum class PayloadType : std::int32_t {
  INIT_RECORDER, INIT_ITEM, DATA, };

struct PACKED PayloadFrame {
  PayloadFrame(int32_t id, PayloadType type)
      : recorder_id(id)
      , payload_type(type) {
  }
  int32_t recorder_id;
  PayloadType payload_type;
};

struct PACKED RecorderInit {
  RecorderInit(int32_t id, int32_t size, std::string name)
      : recorder_id(id)
      , recorder_item_size(size) {
    std::strncpy(recorder_name, name.c_str(), sizeof(recorder_name));
  }
  int32_t recorder_id;
  int32_t recorder_item_size;
  char recorder_name[56];
};

enum class ItemType : std::int16_t {
  INIT, OTHER, CHAR, INT, UINT, FLOAT, STR, };

struct PACKED ItemInit {
  ItemInit(int16_t key,
           std::string const& name,
           std::string const& unit,
           std::string const& desc);

  int16_t key;
  char name[32];
  char unit[32];
  char desc[190];
};

struct PACKED Item {
  Item();
  explicit Item(int16_t key);

  int16_t key;
  ItemType type;
  int32_t time;
  union Data {
    char     c;
    char     s[sizeof(int64_t)];
    int64_t  i;
    uint64_t u;
    double   d;
  } data;
};

CHECK_POW2_SIZE(PayloadFrame);
CHECK_POW2_SIZE(RecorderInit);
CHECK_POW2_SIZE(ItemInit);
CHECK_POW2_SIZE(Item);
// ----------------------------------------------------------------------------


template<typename V> void
updateItem(Item* item, int64_t const time, V const value);


class RecorderCommon {
 public:
  RecorderCommon(RecorderCommon const&) = delete;
  RecorderCommon& operator= (RecorderCommon const&) = delete;

  static int constexpr SEND_BUFFER_SIZE = 1<<10;
  typedef std::array<Item, SEND_BUFFER_SIZE> SendBuffer;

  RecorderCommon(int32_t id, std::string name);
  ~RecorderCommon();

  //!  Set class context to use for ZeroMQ communication.
  static void setContext(zmq::context_t* context);

  //!  Set class socket address for ZeroMQ communication.
  static void setAddress(std::string address);

  //!  Get socket address.
  static std::string getAddress();

 protected:
  void flushSendBuffer();

  void setup(ItemInit const& init);

  void record(Item const& item);

  static zmq::context_t* socket_context;
  static std::string     socket_address;

 private:
  // Local identifier for the recorder. This goes into the first frame
  // of the zeromq message for the backend to use as filtering and
  // sorting identification.
  int32_t const recorder_id_;
  std::string const recorder_name_;

  std::unique_ptr<zmq::socket_t> socket_;
  static thread_local SendBuffer send_buffer;
  static thread_local SendBuffer::size_type send_buffer_index;
};


template<typename K>
class Recorder : public RecorderCommon {
 public:
  Recorder(Recorder const&) = delete;
  Recorder& operator= (Recorder const&) = delete;

  typedef RecorderCommon Super;

  Recorder(int32_t id, std::string name)
      : Super(id, name) {
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
