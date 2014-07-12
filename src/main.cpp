// -*- mode:c++; indent-tabs-mode:nil; -*-

#include "Recorder.hh"

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
#include <cassert>
#include <future>
#include <unordered_map>
#include <type_traits>

namespace {
typedef std::chrono::milliseconds msec;
}

class Component
{
  static zmq::context_t* context;
  static std::string socket_address;

 public:
  Component(uint32_t component_id)
      : _component_id(component_id),
        _iteration(0)
  {
    assert(Component::context != nullptr
           && "Class context must be set via setContext()");
    assert(!Component::socket_address.empty()
           && "Class address must be set via setAddress()");

    _socket.reset(new zmq::socket_t(*Component::context, ZMQ_PUSH));
    _socket->connect(Component::socket_address.c_str());

    _recorder.reset(new Recorder(_component_id));
  }

  ~Component() {
    _socket->close();
  }

  void calculate() {
    zmq::message_t msg;
    char buf[32];
    int64_t sum = 0;
    for (int64_t i=0; i<1e3; ++i) {
      for (int64_t j=0; j<1e3; ++j) {
        sum += i + j;
      }
    }

    // _recorder->record("rdr1", sum);
    // _recorder->record("on/off", sum);

    snprintf(buf, sizeof(buf), "%03d:%03d -- %ld", _component_id, ++_iteration, sum);
    msg.rebuild(sizeof(buf));
    memcpy(msg.data(), &buf, sizeof(buf));
    _socket->send(msg);
  }

  static void setContext(zmq::context_t* ctx) {
    context = ctx;
  }

  static void setAddress(std::string addr) {
    socket_address = addr;
  }

  std::unique_ptr<Recorder> _recorder;

 private:
  int32_t _component_id;
  int32_t _iteration;
  std::unique_ptr<zmq::socket_t> _socket;

};

zmq::context_t* Component::context = nullptr;
std::string     Component::socket_address;


void producer(zmq::context_t* ctx)
{
  zmq::socket_t vent(*ctx, ZMQ_PUSH);
  zmqutils::apply(zmqutils::connect, &vent, "inproc://sink");

  std::stringstream thread_id_string;
  thread_id_string << std::this_thread::get_id();

  std::srand(std::time(0));

  zmq::message_t msg;
  for (int i=0; i<3; ++i) {
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
  zmqutils::apply(zmqutils::bind, &sink, "inproc://sink");

  bool polling_data = true;
  int num_messages = 0;
  zmq::message_t msg;
  zmq_pollitem_t pollitems[] = { { sink, 0, ZMQ_POLLIN, 0 } };
  while (running->load() || polling_data) {
    if (!zmqutils::poll(pollitems)) {
      polling_data = false;
      continue;
    } else {
      sink.recv(&msg);
      ++num_messages;
    }

    if (num_messages % 1 == 0) {
      std::cout << __func__ << ":(" << std::setw(8) << num_messages << ")"
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

  Component::setContext(&ctx);
  Component::setAddress("inproc://sink");

  Component c1(1);
  Component c2(2);
  std::function<void(Component*)> fcalc = &Component::calculate;
  for (int i=0; i<3; ++i)
  {
    fcalc(&c1);
    fcalc(&c2);
  }
  running_consumer.store(false);
  th_consumer.join();

  std::this_thread::sleep_for(msec(20));

  Recorder rec1(100);
  Recorder rec2(201);
  rec1.setup("foo","m");
  rec2.setup("foo","m/s");
  rec2.setup("fiii","s");
  rec1.record("foo",1.1);
  rec1.record("foo",1.1);
  rec2.record("fiii",9.9);
  rec2.record("fiii",9.9);
  rec2.record("foo",9.1);
  rec2.record("foo",9.1);
  rec1.record("foo",1.1);
  rec1.record("foo",1.1);
  rec1.record("foo",1.2);
  rec1.record("foo",1.2);
  rec1.record("foo",1.2);
  rec1.record("foo",1.2);
  rec2.record("foo",9.1);
  rec2.record("foo",9.2);
  rec2.record("foo",9.3);
  rec2.record("foo",9.4);
  rec2.record("foo",9.4);
  rec2.record("foo",9.4);
  rec2.record("foo",9.4);
  rec1.record("foo",1.2);
  rec1.record("foo",1.2);
  rec1.record("foo",1.2);
  rec2.record("fiii",5.5);
  rec1.record("foo",1.2);
  rec1.record("foo",1.2);
  rec2.record("foo",9.4);
  rec2.record("foo",9.4);
  rec1.record("foo",1.2);
  rec1.record("foo",1.0);
  rec2.record("fiii",3.3);
  rec1.record("foo",1.0);
  rec1.record("foo",1.0);
  rec1.record("foo",1.0);
  rec1.record("foo",1.0);
  rec1.record("foo",1.1);

  return 0;
}
