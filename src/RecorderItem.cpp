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

#include "Recorder.h"

#include <string>
#include <cstdio>

Item::Item()
    : type(Type::INIT)
    , time(-1) {
  std::memset(&data, 0, sizeof(data));
}

Item::Item(std::string const& n, std::string const& u)
    : Item() {
  std::strncpy(name, n.c_str(), sizeof(name));
  std::strncpy(unit, u.c_str(), sizeof(unit));
}

std::string
Item::toString() const {
  char buffer[128];
  std::string name(this->name);
  std::string unit(this->unit);
  unit = "[" + unit + "]";
  snprintf(buffer, sizeof(buffer), "%ld %10s = ", this->time, name.c_str());
  switch (this->type) {
    case Type::CHAR:
      snprintf(buffer, sizeof(buffer), "%c", data.c);
      break;
    case Type::INT:
      snprintf(buffer, sizeof(buffer), "%ld", data.i);
      break;
    case Type::UINT:
      snprintf(buffer, sizeof(buffer), "%lu", data.u);
      break;
    case Type::FLOAT:
      snprintf(buffer, sizeof(buffer), "%f", data.d);
      break;
    case Type::STR:
      {
        std::string str(data.s, sizeof(data.s));
        snprintf(buffer, sizeof(buffer), "%s", str.c_str());
      }
      break;
    default:
      break;
  }
  snprintf(buffer, 2 + sizeof(this->unit), "%s\n", unit.c_str());
  return std::string(buffer);
}
