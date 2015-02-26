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

// RecorderBase shall simplify the sharing of socket addresses and
// communication infrastructure for producers and recorders. Both
// context and sink address shall be the same for all instances.
class RecorderBase {
 public:
  RecorderBase(RecorderBase const&) = delete;
  RecorderBase& operator= (RecorderBase const&) = delete;

  // Fixed size send buffer to decrese the number of zmq send
  // operations.
  static int constexpr SEND_BUFFER_SIZE = 1<<10;
  typedef std::array<Item, SEND_BUFFER_SIZE> SendBuffer;

  // Ctor takes a string name and integer id for external
  // identification. The string is for having a easy-to-read name, the
  // int id is for fast lookup if needed.
  RecorderBase(std::string const& name, int32_t external_id = 0);
  ~RecorderBase();

  // Set context to use for ZeroMQ communication. The purpose is to
  // share a single zmq context between all threads.
  static void setContext(zmq::context_t* context);

  // Socket sink address for ZeroMQ communication. This is the
  // destination for all recorder instances. On the other end a receiver
  // of the data shall listen and write it elsewhere.
  static void setAddress(std::string const& address);

  // Get socket address.
  static std::string getAddress();

  // Stop all operations (by closing the socket).
  static void shutDown();

 protected:
  void flushSendBuffer();

  // Sets up the recorder instance by sending data to the backend with
  // "suitable" data. This data shall be used for sorting the source of
  // future item data.
  void setupRecorder(int32_t max_size);

  // Sets up each item to record. This enables the backend to predict
  // the amount of different items possible.
  void setupItem(InitItem const& init);

  // Send implementation that either append to the buffer or flushes it
  // if necessary.
  void record(Item const& item);

  static zmq::context_t* socket_context;
  static std::string     socket_address;

  // Local/internal identifer for the recorder. This goes into the first
  // frame of the zeromq message for the backend to use for filtering
  // and sorting identification. The recorder_id_ is a generated
  // incremented internal number and can (should) be used as an array
  // index by the backend.
  int16_t const recorder_id_;

  // External id and a readable name for the recorder which are only
  // sent in the setup message. The backend should map the internal id
  // to the external.
  int64_t const external_id_;
  std::string const recorder_name_;

 private:
  static thread_local std::shared_ptr<zmq::socket_t> socket_;

  SendBuffer send_buffer;
  SendBuffer::size_type send_buffer_index;
};
