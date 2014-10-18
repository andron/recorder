// -*- mode:c++; indent-tabs-mode:nil; -*-

#include <array>
#include <cstdio>
#include <unordered_map>
#include <utility>
#include <typeinfo>
#include <iostream>

#define M(msg) "\n ==== " msg " ===="
#define PACKED __attribute__((packed))

struct PACKED RecItem {
  int8_t type;
  int8_t meta;
  int64_t data;
};

template <size_t I, typename T, size_t N>
static auto get(std::array<T, N>& ary)
    -> typename std::array<T, N>::value_type&
{
  static_assert(I<N, "Out of range");
  return ary[I];
}

template<typename T>
class Recorder {
  static_assert(std::is_enum<T>::value,
                M("Template type T must be enum class type"));

 public:
  Recorder() {
  }
  
  ~Recorder() {
  }

  template<typename V>
  void record(T key, V value) {
    static_assert(std::is_pod<V>::value,
                  M("Template type V must be POD type"));

    RecItem item;
    item.data = value;
    items[static_cast<size_t>(key)] = item;
  }

  void print() const {
    size_t idx = 0;
    for (auto& item : items) {
      printf(" -- %2c %2c -- %ld\n", item.type, item.meta, item.data);
      ++idx;
    }
  }

  static auto constexpr Length = static_cast<size_t>(T::Count);
  std::array<RecItem, Length> items;
};

enum class FOO { A, B, C, D, Count };
enum class BAR { X, Y, Z, Count };

int
main(int, char**) {

  printf("%4lu\n", sizeof(RecItem));
  printf("%4d\n", FOO::Count);
  printf("%4d\n", BAR::Count);

  Recorder<FOO> recfoo;
  printf("%4lu\n", recfoo.items.size());

  recfoo.record(FOO::A, -1.0);
  recfoo.record(FOO::B, 1.0);
  recfoo.record(FOO::C, 2.1);
  recfoo.record(FOO::D, 44);

  recfoo.print();

  Recorder<BAR> recbar;
  recbar.record(BAR::X, -65.0);
  recbar.record(BAR::Y, 21.0);
  recbar.record(BAR::Z, 32.1);
  printf("%4lu\n", recbar.items.size());

  auto& x = ::get<1>(recbar.items);
  std::cout << x.data << std::endl;

  x.data = 99.99;

  std::cout << typeid(x).name() << std::endl;
  std::cout << x.data << std::endl;

  auto& y = ::get<1>(recbar.items);
  std::cout << y.data << std::endl;

  y.data = 200.1;

  auto z = ::get<3>(recfoo.items);
  std::cout << z.data << std::endl;
  
  return 0;
}
