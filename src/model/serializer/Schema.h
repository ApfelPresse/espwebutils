#pragma once

#include <tuple>

namespace fj {

template <typename T, typename MemberT>
struct Field {
  const char* key;
  MemberT T::* member;
};

template <typename T, size_t N>
struct FieldStr {
  const char* key;
  char (T::* member)[N];
};

// ============================================================================
// Schema and Field reflection-like containers
// ============================================================================
// Minimal metadata for mapping struct members to JSON keys during serialization

template <typename T, typename... Fs>
struct Schema {
  std::tuple<Fs...> fields;
};

template <typename T, typename... Fs>
inline Schema<T, Fs...> makeSchema(Fs... fs) { return { std::make_tuple(fs...) }; }

// ---------------------------------------------------------------------------
// tuple_for_each (C++11)
// ---------------------------------------------------------------------------

template <size_t I, typename Tuple, typename Func>
struct TupleForEach {
  static void apply(const Tuple& t, Func& f) {
    TupleForEach<I - 1, Tuple, Func>::apply(t, f);
    f(std::get<I>(t));
  }
};

template <typename Tuple, typename Func>
struct TupleForEach<0, Tuple, Func> {
  static void apply(const Tuple& t, Func& f) { f(std::get<0>(t)); }
};

template <typename Tuple, typename Func>
inline void tuple_for_each(const Tuple& t, Func& f) {
  constexpr size_t N = std::tuple_size<Tuple>::value;
  TupleForEach<N - 1, Tuple, Func>::apply(t, f);
}

} // namespace fj
