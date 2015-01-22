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

#include "zmqutils.h"

#include <zmq.hpp>

#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdlib>
#include <functional>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <sstream>
#include <string>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <vector>
#include <mutex>

namespace {

typedef std::chrono::milliseconds msec;

static std::random_device rd;
static std::mt19937 gen(rd());

struct SocketDeleter {
  void operator() (zmq::socket_t* ptr) {
    ptr->close();
  }
};

struct ClientState {
  int32_t id;
  int32_t freq;
  std::unique_ptr<zmq::socket_t, SocketDeleter> socket;
  ClientState() : id(0), freq(1) {}
};

class StateServer {
 public:
  explicit StateServer(zmq::context_t* ctx)
      : context_(ctx)
      , running_(false)
  {
    printf("%s\n", __func__);
    control_socket_ = new zmq::socket_t(*context_, ZMQ_REP);
    zmqutils::apply(zmqutils::fbind, control_socket_, "tcp://127.0.0.1:10000");
  }

  ~StateServer() {
    printf("%s\n", __func__);
    control_socket_->close();
    delete control_socket_;
  }

  void run() {
    running_.store(true);
    workers_.emplace_back(std::thread(&StateServer::runControl, this));
    workers_.emplace_back(std::thread(&StateServer::runUpdater, this));
    std::this_thread::sleep_for(msec(100));
  }

  void quit() {
    running_.store(false);
    for (size_t i = 0; i < workers_.size(); ++i) {
      if (workers_[i].joinable()) {
        workers_[i].join();
      }
    }
  }

 protected:
  void runControl() {
    printf("%s\n", __func__);
    zmq::message_t zmsg(1024);
    zmq_pollitem_t pollitems[] = { { *control_socket_, 0, ZMQ_POLLIN, 0 } };
    while (running_.load()) {
      if (!zmqutils::poll(pollitems)) {
        continue;
      }

      control_socket_->recv(&zmsg);

      auto msg = zmqutils::to_string(zmsg);

      if (msg.empty()) {
        printf("MSG = EMPTY\n");
        continue;
      } else {
        printf("MSG = #%s#\n", msg.c_str());
      }

      std::string cmd;
      std::string arg;

      {
        char scan_cmd[32];
        char scan_arg[64];
        auto rval = std::sscanf(msg.c_str(), "%s %s", scan_cmd, scan_arg);
        if (rval > 0)
          cmd = std::string(scan_cmd);
        if (rval > 1)
          arg = std::string(scan_arg);
      }
      printf("  CMD = #%s#\n", cmd.c_str());
      printf("  ARG = #%s#\n", arg.c_str());

      std::string reply;
      if (cmd == "connect") {
        connectClient(&reply);
      }
      else if (cmd == "disconnect") {
        disconnectClient(&reply);
      }
      else if (cmd == "freq") {
        updateClientFrequency(arg, &reply);
      }
      else {
        reply = std::string(cmd.crbegin(), cmd.crend());
      }

      printf("REP = #%s#\n", reply.c_str());

      control_socket_->send(reply.data(), reply.size());
    }
  }

  void connectClient(std::string* const reply) {
    static int32_t client_id = 1000;
    auto* cs = new ClientState;
    cs->id = ++client_id;
    cs->socket.reset(new zmq::socket_t(*context_, ZMQ_PUSH));
    zmqutils::apply(zmqutils::fbind, cs->socket.get(), "tcp://127.0.0.1:*");

    //std::lock_guard<std::mutex> lock(clients_mutex_);
    clients_.push_back(cs);

    auto const address = zmqutils::get_address(cs->socket.get());
    std::stringstream ss;
    ss << client_id << " " << address;
    (*reply) = ss.str();
  }

  void disconnectClient(std::string* const reply) {
    //std::lock_guard<std::mutex> lock(clients_mutex_);
    if (clients_.empty()) {
      (*reply) = "N/A";
      return;
    }
    auto elem = clients_.begin();
    std::stringstream ss;
    ss << "disconnect " << (*elem)->id;
    (*reply) = ss.str();
    clients_.erase(elem);
    delete (*elem);
  }

  void updateClientFrequency(std::string const& arg, std::string* reply) {
    //std::lock_guard<std::mutex> lock(clients_mutex_);
    if (clients_.empty()) {
      (*reply) = "N/A";
      return;
    }

    std::uniform_int_distribution<> dist(0, clients_.size()-1);
    auto const idx = dist(gen);
    int f;
    try {
      f = std::stoi(arg);
    } catch(...) {
      printf("ARG = %s\n", arg.c_str());
      f = 10;
    }
    clients_[idx]->freq = f;

    std::stringstream ss;
    ss << "frequency " << clients_[idx]->id << " = " << f << "Hz";
    (*reply) = ss.str();
  }

  void runUpdater() {
    uint64_t loop = 0;
    printf("%s\n", __func__);
    while (running_.load()) {
      std::this_thread::sleep_for(msec(250));
      std::lock_guard<std::mutex> lock(clients_mutex_);
      for (auto const client : clients_) {
        if (client == nullptr)
          continue;
        if (loop % client->freq != 0)
          continue;

        std::stringstream ss;
        ss << "data:" << loop;
        client->socket->send(ss.str().c_str(), ss.str().size());
      }
      ++loop;
    }
  }

 private:
  zmq::context_t* context_;

  zmq::socket_t* control_socket_;

  std::atomic<bool> running_;
  std::vector<std::thread> workers_;

  std::vector<ClientState*> clients_;
  std::mutex clients_mutex_;
};

}


int
main(int, char**) {
  zmq::context_t ctx(1);

  StateServer server(&ctx);

  server.run();

  std::cout << "Press [enter] to exit" << std::endl << std::flush;
  getchar();

  server.quit();

  return 0;
}
