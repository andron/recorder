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

#include <zmq.hpp>

#include <string>
#include <memory>

namespace zmqutils {

inline bool
poll(zmq_pollitem_t* items) {
  constexpr static int POLL_INTERVALL = 100;
  int const rval= zmq_poll(items, 1, POLL_INTERVALL);
  switch (rval) {
    case -1:
      perror("zmq_poll");
      std::abort();
      break;;
    case 0:
      return false;
      break;;
    default:
      return true;
      break;;
  }
}

// inline int
// send(void* socket, QByteArray const& msg) {
//   zmq_msg_t z_msg;
//   zmq_msg_init_size(&z_msg, msg.size());
//   memcpy(zmq_msg_data(&z_msg), msg.data(), msg.size());
//   int rval = zmq_send(socket, &z_msg, 0);
//   if (rval != 0) {
//     perror("zmq_send");
//   }
//   zmq_msg_close(&z_msg);
//   return rval;
// }

// inline int
// recv(void* socket, QByteArray* msg, int options = 0) {
//   zmq_msg_t z_msg;
//   zmq_msg_init(&z_msg);
//   int rval = zmq_recv(socket, &z_msg, options);
//   if (rval != 0) {
//     perror("zmq_recv");
//     zmq_msg_close(&z_msg);
//     return rval;
//   }
//   msg->resize(zmq_msg_size(&z_msg));
//   memcpy(msg->data(), zmq_msg_data(&z_msg), msg->size());
//   zmq_msg_close(&z_msg);
//   return rval;
// }

typedef std::function<void(zmq::socket_t*, char const*)> socket_func;

static socket_func fconnect = &zmq::socket_t::connect;
static socket_func fbind    = &zmq::socket_t::bind;

inline void
apply(socket_func const& function, zmq::socket_t* socket, char const* address) {
  try {
    function(socket, address);
  } catch (zmq::error_t& e) {
    std::fprintf(stderr, "Error:%s: %s", __func__, e.what());
    std::exit(1);
  }
}

inline void
connect(zmq::socket_t* socket, std::string const& address) {
  apply(fconnect, socket, address.c_str());
}

inline void
bind(zmq::socket_t* socket, std::string const& address) {
  apply(fbind, socket, address.c_str());
}

inline std::string
to_string(zmq::message_t const& zmsg) {
  std::vector<char> buffer(zmsg.size());
  memcpy(static_cast<void*>(&buffer[0]), zmsg.data(), zmsg.size());
  return std::string(buffer.begin(), buffer.end());
}

inline zmq::message_t
to_message(std::string const&) {
  zmq::message_t zmsg;
  return zmsg;
}

inline std::string
get_address(zmq::socket_t& zsock) {
  std::vector<char> optbuf(256);
  auto optlen = optbuf.size();
  zsock.getsockopt(ZMQ_LAST_ENDPOINT, static_cast<void*>(&optbuf[0]), &optlen);
  return std::string(optbuf.begin(), optbuf.begin() + optlen);
}

}  // namespace zmqutils
