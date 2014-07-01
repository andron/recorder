// -*- mode:c++; indent-tabs-mode:nil; -*-

#include <zmq.hpp>

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <functional>
#include <thread>
#include <atomic>
#include <sstream>
#include <numeric>
#include <iomanip>

namespace {

typedef std::chrono::milliseconds msec;

inline bool poll(zmq_pollitem_t* items)
{
  constexpr static int POLL_INTERVALL = 200;
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

namespace socket {

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

}}


void producer(zmq::context_t* ctx)
{
  zmq::socket_t vent(*ctx, ZMQ_PUSH);
  socket::apply(socket::connect, &vent, "inproc://sink");

  std::stringstream thread_id_string;
  thread_id_string << std::this_thread::get_id();

  std::srand(std::time(0));

  zmq::message_t msg;
  for (int i=0; i<10000; ++i) {
    msg.rebuild(1024);
    snprintf(static_cast<char*>(msg.data()), msg.size(),
             "%s:%d ... abcdefghijklmnopqrstuvxyz ABCDEFGHIJKLMNOPQRSTUVXYZ",
             thread_id_string.str().c_str(), i);
    vent.send(msg);
    std::this_thread::sleep_for(msec(2));
  }
}

void consumer(zmq::context_t* ctx, std::atomic<bool> const* running)
{
  zmq::socket_t sink(*ctx, ZMQ_PULL);
  socket::apply(socket::bind, &sink, "inproc://sink");

  int num_messages = 0;
  zmq::message_t msg(32);
  zmq_pollitem_t pollitems[] = { { sink, 0, ZMQ_POLLIN, 0 } };
  while (running->load()) {
    if (!poll(pollitems)) {
      continue;
    } else {
      sink.recv(&msg);
      ++num_messages;
    }

    if (num_messages % 5000 == 0) {
      std::cout << __func__ << ":(" << std::setw(10) << num_messages << ")"
                << static_cast<char*>(msg.data()) << std::endl;
    }
  }
}

int
main(int ac, char** av)
{
  int num_threads = 2;
  if (ac > 1)
    num_threads = std::atoi(av[1]);

  zmq::context_t ctx(1);
  std::vector<std::thread> workers;

  std::atomic<bool> running_consumer(true);
  auto th_consumer = std::thread(consumer, &ctx, &running_consumer);

  std::this_thread::sleep_for(msec(2));

  for (int i=0; i<num_threads; ++i) {
    workers.emplace_back(std::thread(producer, &ctx));
  }

  for (auto& w : workers) {
    w.join();
  }

  running_consumer.store(false);
  th_consumer.join();

  return 0;
}
