#pragma once

#include "fl/cstddef.h"
#include "fl/utility.h"
#include "fl/type_traits.h"

namespace fl {

// Forward declaration
template<typename... Ts> struct tuple;

// Empty-tuple specialization
template<>
struct tuple<> {};

// Recursive tuple: head + tail
template<typename Head, typename... Tail>
struct tuple<Head, Tail...> {
    Head head;
    tuple<Tail...> tail;

    tuple() = default;

    tuple(const Head& h, const Tail&... t)
      : head(h), tail(t...) {}

    tuple(Head&& h, Tail&&... t)
      : head(fl::move(h)), tail(fl::forward<Tail>(t)...) {}
};

// tuple_size
template<typename T>
struct tuple_size;

template<typename... Ts>
struct tuple_size< tuple<Ts...> > : integral_constant<size_t, sizeof...(Ts)> {};

// tuple_element
template<size_t I, typename Tuple>
struct tuple_element;

template<typename Head, typename... Tail>
struct tuple_element<0, tuple<Head, Tail...>> {
    using type = Head;
};

template<size_t I, typename Head, typename... Tail>
struct tuple_element<I, tuple<Head, Tail...>>
  : tuple_element<I-1, tuple<Tail...>> {};

// get<I>(tuple)
template<size_t I, typename Head, typename... Tail>
typename enable_if<I == 0, Head&>::type
get(tuple<Head, Tail...>& t) {
    return t.head;
}

template<size_t I, typename Head, typename... Tail>
typename enable_if<I != 0, typename tuple_element<I, tuple<Head, Tail...>>::type&>::type
get(tuple<Head, Tail...>& t) {
    return get<I-1>(t.tail);
}

// const overloads
template<size_t I, typename Head, typename... Tail>
typename enable_if<I == 0, const Head&>::type
get(const tuple<Head, Tail...>& t) {
    return t.head;
}

template<size_t I, typename Head, typename... Tail>
typename enable_if<I != 0, const typename tuple_element<I, tuple<Head, Tail...>>::type&>::type
get(const tuple<Head, Tail...>& t) {
    return get<I-1>(t.tail);
}

// rvalue overloads
template<size_t I, typename Head, typename... Tail>
typename enable_if<I == 0, Head&&>::type
get(tuple<Head, Tail...>&& t) {
    return fl::move(t.head);
}

template<size_t I, typename Head, typename... Tail>
typename enable_if<I != 0, typename tuple_element<I, tuple<Head, Tail...>>::type&&>::type
get(tuple<Head, Tail...>&& t) {
    return get<I-1>(fl::move(t.tail));
}

// make_tuple
template<typename... Ts>
tuple<typename fl::decay<Ts>::type...>
make_tuple(Ts&&... args) {
    return tuple<typename fl::decay<Ts>::type...>(fl::forward<Ts>(args)...);
}

} // namespace fl