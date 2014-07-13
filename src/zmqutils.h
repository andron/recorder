// -*- mode:c++; indent-tabs-mode:nil; -*-

#pragma once

#include <zmq.hpp>

#include <string>
#include <memory>
#include <iostream>

namespace zmqutils {

inline bool poll(zmq_pollitem_t* items)
{
  constexpr static int POLL_INTERVALL = 100;
  int const rval= zmq_poll(items, 1, POLL_INTERVALL);
  switch (rval) {
    case -1:
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

typedef std::function<void(zmq::socket_t*, char const*)> SockFunction;

SockFunction connect = &zmq::socket_t::connect;
SockFunction bind    = &zmq::socket_t::bind;

void apply(SockFunction const& function, zmq::socket_t* socket, char const* address)
{
  try {
    function(socket, address);
  } catch (zmq::error_t& e) {
    std::cout << __func__ << ":" << e.what() << std::endl;
    std::exit(1);
  }
}

class Socket {
 public:
  ~Socket() {}
 protected:
  Socket() {}
  Socket(Socket const&) = delete;

  static zmq::context_t* socket_context;
  static std::string     socket_address;

  static void setContext(zmq::context_t* ctx) { socket_context = ctx; }
  static void setAddress(std::string addr)    { socket_address = addr; }

  std::unique_ptr<zmq::socket_t> socket;
};
zmq::context_t* Socket::socket_context = nullptr;
std::string     Socket::socket_address;

}
