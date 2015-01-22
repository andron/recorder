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

#include "RecorderTypes.h"

#include <array>
#include <memory>
#include <string>

namespace zmq {
class context_t;
class socket_t;
}

class RecorderBase {
 public:
  RecorderBase(RecorderBase const&) = delete;
  RecorderBase& operator= (RecorderBase const&) = delete;

  static int constexpr SEND_BUFFER_SIZE = 1<<10;
  typedef std::array<Item, SEND_BUFFER_SIZE> SendBuffer;

  RecorderBase(std::string name, int32_t external_id = 0);
  ~RecorderBase();

  // Set class context to use for ZeroMQ communication.
  static void setContext(zmq::context_t* context);

  // Set class socket address for ZeroMQ communication.
  static void setAddress(std::string address);

  // Get socket address.
  static std::string getAddress();

  // Stop all operations.
  static void shutDown();

 protected:
  void flushSendBuffer();

  void setupRecorder(int32_t max_size);

  void setupItem(InitItem const& init);

  void record(Item const& item);

  static zmq::context_t* socket_context;
  static std::string     socket_address;

  // Local identifier for the recorder. This goes into the first frame
  // of the zeromq message for the backend to use as filtering and
  // sorting identification.
  int64_t const external_id_;
  int16_t const recorder_id_;
  std::string const recorder_name_;

 private:
  static thread_local std::shared_ptr<zmq::socket_t> socket_;

  SendBuffer send_buffer;
  SendBuffer::size_type send_buffer_index;
};
