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

ItemInit::ItemInit(int16_t item_key,
                   std::string const& item_name,
                   std::string const& item_unit,
                   std::string const& item_desc)
    : key(item_key) {
  std::strncpy(name, item_name.c_str(), sizeof(name));
  std::strncpy(unit, item_unit.c_str(), sizeof(unit));
  std::strncpy(desc, item_desc.c_str(), sizeof(desc));
}

Item::Item()
    : key(-1)
    , type(ItemType::INIT)
    , time(-1) {
  std::memset(&data, 0, sizeof(data));
}

Item::Item(int16_t item_key)
    : Item() {
  key = item_key;
}