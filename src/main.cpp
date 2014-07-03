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
#include <cassert>
#include <future>
#include <unordered_map>

namespace {

typedef std::chrono::milliseconds msec;

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


class Recorder
{
  uint32_t _object_key;

  template<typename T>
  struct RecordItem {
    int64_t time;
    T value;
  };

 public:
  Recorder() = delete;
  Recorder(Recorder const&) = delete;
  explicit Recorder(uint32_t object_key) : _object_key(object_key) {}
  ~Recorder() {}

  template<typename T>
  void send(RecordItem<T> const& item) const {
    std::cout << "rec:" << _object_key << " -- " << item.time << ":" << item.value << std::endl << std::flush;
  }

  template<typename T>
  void record(int key, T const& value) const {
    static std::unordered_map<int, RecordItem<T>> storage;
    auto it = storage.find(key);
    if (it == storage.end()) {
      RecordItem<T> item = {std::clock(), value};
      storage.emplace(key, item);
      this->send(item);
    } else if (it->second.value != value) {
      // Set flank time
      it->second.time = std::clock();
      // Send old value, i.e. flank start value
      this->send(it->second);
      // Set new value
      it->second.value = value;
      // Send new value, i.e. flank end value
      this->send(it->second);
    } else {
      // Ignore if value does not change
    }
  }
};


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

    _recorder->record(100, sum);
    _recorder->record(101, sum);

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
  socket::apply(socket::connect, &vent, "inproc://sink");

  std::stringstream thread_id_string;
  thread_id_string << std::this_thread::get_id();

  std::srand(std::time(0));

  zmq::message_t msg;
  for (int i=0; i<5; ++i) {
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

  bool polling_data = true;
  int num_messages = 0;
  zmq::message_t msg;
  zmq_pollitem_t pollitems[] = { { sink, 0, ZMQ_POLLIN, 0 } };
  while (running->load() || polling_data) {
    if (!poll(pollitems)) {
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
  for (int i=0; i<20; ++i)
  {
    fcalc(&c1);
    fcalc(&c2);
  }
  running_consumer.store(false);
  th_consumer.join();

  Recorder rec(99);
  rec.record(100,1.0);
  rec.record(100,1.0);
  rec.record(100,1.1);
  rec.record(100,1.1);
  rec.record(100,1.2);
  rec.record(100,1.2);
  rec.record(100,1.2);
  rec.record(100,1.2);
  rec.record(100,1.2);
  rec.record(100,1.2);
  rec.record(100,1.2);
  rec.record(100,1.2);
  rec.record(100,1.2);
  rec.record(100,1.2);
  rec.record(100,1.0);
  rec.record(100,1.0);
  rec.record(100,1.0);
  rec.record(100,1.0);
  rec.record(100,1.0);

  return 0;
}
