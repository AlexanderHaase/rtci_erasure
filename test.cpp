#include "Erasure.hpp"
#include <iostream>

template < typename T >
void tprint( T && t ) { std::cout << typeid(t).name() << ", "; }

template < typename First, typename ...Rest >
void tprint( First && first, Rest && ...rest )
{
  tprint( std::forward<First>( first ) );
  tprint( std::forward<Rest>( rest )... );
}

template < typename ...Args >
struct List
{
  template < template < typename... > class T >
  struct inject
  {
    using type = T<Args...>;
  };

  template < template < typename... > class T >
  using inject_t = typename inject<T>::type;

  template < template < typename > class T >
  struct map
  {
    using type = List<T<Args>...>;
  };

  template < template < typename > class T >
  using map_t = typename map<T>::type;

  /*template < template < typename, typename > class Reduce >
  struct reduce
  {
    using type = Reduce<
  };*/
};

/// Labels for incremental type modifications
///
enum class type_modifier
{
  POINTER,                ///< e.g. T*
  ARRAY,                  ///< e.g. T[N]
  CONST,                  ///< e.g. const T
  LVALUE_REFERENCE,       ///< e.g. T &
  RVALUE_REFERENCE,       ///< e.g. T &&
  VOLATILE,               ///< e.g. volatile T
  NONE                    ///< e.g. T
};

/// Sequentially detect, decode, and copy type modifiers.
///
/// @tparam T type to probe and enumerate.
///
template < typename T >
struct type_traits
{
  using base = T;                                                 ///< Base type without any modifiers.
  using type = T;                                                 ///< Current type as detected.
  static constexpr type_modifier modifier = type_modifier::NONE;  ///< Current modifier.
  static constexpr size_t count = 0;                              ///< Number of modifiers.
  
  /// Apply the current modifier to the specified type.
  ///
  /// @tparam U type to modify
  ///
  template < typename U >
  using modify = U;

  /// Copy modifiers to another arbitrary type.
  ///
  /// @tparam U type to copy modifiers to.
  ///
  template < typename U >
  using rebase = U;

  /// Determine if the specified type is this type.
  ///
  /// @tparam R type to test.
  ///
  template < typename R >
  static constexpr bool is_same = std::is_same<R,type>::value;

  /// Determine if the specified type is contained within this type.
  ///
  /// @tparam R type to test.
  ///
  template < typename R >
  static constexpr bool contains = std::is_same<R,type>::value;

  /// Copy modifiers up to the specified base
  ///
  /// @tparam R type to use as a base for rewriting.
  /// @tparam U type to rebase from R.
  ///
  template < typename R, typename U >
  using rebase_from = std::conditional_t<is_same<R>, U, void>;
};

/// Specialization for pointer modifier.
///
template < typename T >
struct type_traits<T*>
{
  using next = type_traits<T>;
  using base = typename next::base;
  using type = T*;
  static constexpr type_modifier modifier = type_modifier::POINTER;
  static constexpr size_t count = next::count + 1;

  template < typename U >
  using modify = std::add_pointer_t< U >;

  template < typename U >
  using rebase = modify< typename next::template rebase<U> >;

  template < typename R >
  static constexpr bool is_same = std::is_same<type, R>::value;

  template < typename R >
  static constexpr bool contains = is_same<R> || next::template contains<R>;

  template < typename R, typename U >
  using rebase_from = std::conditional_t<is_same<R>, U, modify<typename next::template rebase_from<R, U>>>;
};

/// Specialization for lvalue reference modifier.
///
template < typename T >
struct type_traits<T&>
{
  using next = type_traits<T>;
  using base = typename next::base;
  using type = T&;
  static constexpr type_modifier modifier = type_modifier::LVALUE_REFERENCE;
  static constexpr size_t count = next::count + 1;

  template < typename U >
  using modify = std::add_lvalue_reference_t< U >;

  template < typename U >
  using rebase = modify< typename next::template rebase<U> >;

  template < typename R >
  static constexpr bool is_same = std::is_same<type, R>::value;

  template < typename R >
  static constexpr bool contains = is_same<R> || next::template contains<R>;

  template < typename R, typename U >
  using rebase_from = std::conditional_t<is_same<R>, U, modify<typename next::template rebase_from<R, U>>>;
};

/// Specialization for rvalue_reference modifier.
///
template < typename T >
struct type_traits<T&&>
{
  using next = type_traits<T>;
  using base = typename next::base;
  using type = T&&;
  static constexpr type_modifier modifier = type_modifier::RVALUE_REFERENCE;
  static constexpr size_t count = next::count + 1;

  template < typename U >
  using modify = std::add_rvalue_reference_t< U >;

  template < typename U >
  using rebase = modify< typename next::template rebase<U> >;

  template < typename R >
  static constexpr bool is_same = std::is_same<type, R>::value;

  template < typename R >
  static constexpr bool contains = is_same<R> || next::template contains<R>;

  template < typename R, typename U >
  using rebase_from = std::conditional_t<is_same<R>, U, modify<typename next::template rebase_from<R, U>>>;
};

/// Specialization for const modifier.
///
template < typename T >
struct type_traits<const T>
{
  using next = type_traits<T>;
  using base = typename next::base;
  using type = const T;
  static constexpr type_modifier modifier = type_modifier::CONST;
  static constexpr size_t count = next::count + 1;

  template < typename U >
  using modify = std::add_const_t< U >;

  template < typename U >
  using rebase = modify< typename next::template rebase<U> >;

  template < typename R >
  static constexpr bool is_same = std::is_same<type, R>::value;

  template < typename R >
  static constexpr bool contains = is_same<R> || next::template contains<R>;

  template < typename R, typename U >
  using rebase_from = std::conditional_t<is_same<R>, U, modify<typename next::template rebase_from<R, U>>>;
};

/// Specialization for volatile modifier.
///
template < typename T >
struct type_traits<volatile T>
{
  using next = type_traits<T>;
  using base = typename next::base;
  using type = volatile T;
  static constexpr type_modifier modifier = type_modifier::VOLATILE;
  static constexpr size_t count = next::count + 1;

  template < typename U >
  using modify = std::add_volatile_t< U >;

  template < typename U >
  using rebase = modify< typename next::template rebase<U> >;

  template < typename R >
  static constexpr bool is_same = std::is_same<type, R>::value;

  template < typename R >
  static constexpr bool contains = is_same<R> || next::template contains<R>;

  template < typename R, typename U >
  using rebase_from = std::conditional_t<is_same<R>, U, modify<typename next::template rebase_from<R, U>>>;
};

/// Specialization for array modifier.
///
template < typename T, size_t N >
struct type_traits<T[N]>
{
  using next = type_traits<T>;
  using base = typename next::base;
  using type = T[N];
  static constexpr size_t size = N;
  static constexpr type_modifier modifier = type_modifier::ARRAY;
  static constexpr size_t count = next::count + 1;

  template < typename U >
  using modify = U[N];

  template < typename U >
  using rebase = modify< typename next::template rebase<U> >;

  template < typename R >
  static constexpr bool is_same = std::is_same<type, R>::value;

  template < typename R >
  static constexpr bool contains = is_same<R> || next::template contains<R>;

  template < typename R, typename U >
  using rebase_from = std::conditional_t<is_same<R>, U, modify<typename next::template rebase_from<R, U>>>;
};

static_assert( type_traits<int*>::count == 1, "type check!" );
static_assert( std::is_same<type_traits<int*>::rebase<float>,float*>::value, "type check!" );
static_assert( std::is_same<type_traits<int**>::rebase_from<int*, float>,float*>::value, "type check!" );
static_assert( type_traits<volatile int*const*[10]>::count == 5, "type check!" );
static_assert( std::is_same<type_traits<volatile int*const*[10]>::rebase<float>,volatile float*const*[10]>::value, "type check!" );

template < typename Target, typename With >
struct replace {

  template < typename Test >
  struct apply {
    using T1 = type_traits<Target>;
    using T2 = type_traits<Test>;
    using type = std::conditional_t< T2::template contains<Target>, typename T2::template rebase_from<Target, With>, Test >;
  };

  template <typename Test>
  using apply_t = typename apply<Test>::type;

  template < typename Signature >
  struct signature;

  template < typename Return, typename ...Args >
  struct signature<Return(Args...)>
  {
    using type = apply_t<Return>(apply_t<Args>...);
  };

  template < typename Signature >
  using signature_t = typename signature<Signature>::type;
};

struct Placeholder;

template < typename T >
using placeholder_for = replace<Placeholder, T>;

template < typename T >
using is_placeholder = std::is_same<Placeholder, std::decay_t<T>>;

template < typename Base, template < typename, typename > class Template, typename Final, typename Signature, typename ...Remainder>
struct UpdateSequence
{
  using base = typename UpdateSequence<Base, Template, Final, Remainder...>::type;
  using type = Template< base, typename placeholder_for<Final>::template signature_t<Signature> >;
};

template < typename Base, template < typename, typename > class Template, typename Final, typename Signature>
struct UpdateSequence<Base, Template, Final, Signature>
{
  using type = Template< Base, typename placeholder_for<Final>::template signature_t<Signature> >;
};
/*
template < typename First, typename ...Args >
auto get_index( std::index_sequence<0>, First && first, Args && ... )
  -> First
{
  return first;
}

template < size_t Index, typename First, typename ...Args >
auto get_index( std::index_sequence<Index>, First &&, Args && ... args )
  -> decltype(get_index<Index-1, Args...>( std::index_sequence<Index-1>{}, std::forward<Args>(args)... ))
{
  return get_index<Index-1, Args...>( std::index_sequence<Index-1>{}, std::forward<Args>(args)... );
}

template < size_t Select, typename A, typename ...Args >
auto forward_at( std::index_sequence<Select>, std::index_sequence<Select>, A && a, Args && ... )
  -> A
{
  return a;
}

template < size_t Select, size_t Index, typename A, typename ...Args >
auto forward_at( std::index_sequence<Select>, std::index_sequence<Index>, A &&, Args && ...args )
  -> std::enable_if_t<(Select < Index), decltype(get_index(std::index_sequence<Index>{}, std::forward<Args>(args)...))>
{
  return get_index(std::index_sequence<Index>{}, std::forward<Args>(args)...);
}

template < size_t Select, size_t Index, typename A, typename ...Args >
auto forward_at( std::index_sequence<Select>, std::index_sequence<Index>, A &&, Args && ...args )
  -> std::enable_if_t<(Select > Index), decltype(get_index(std::index_sequence<Index+1>{}, std::forward<Args>(args)...))>
{
  return get_index(std::index_sequence<Index+1>{}, std::forward<Args>(args)...);
}
*/

template < typename T >
using Functor = replace<Placeholder,int>::template apply_t<T>;
static_assert( std::is_same<int,Functor<Placeholder>>::value, "owww" );
static_assert( std::is_same<List<int>,List<Placeholder>::map_t<Functor>>::value, "OW" );

template < typename Base, typename Signature >
class FriendImpl;

template < typename Base, typename ...Args>
class FriendImpl<Base, void(Args...)> : public Base {

  using Base::Base;
  friend void foo( Args ... args )
  {
    std::cout << "Foo: ";
    tprint( std::forward<Args>( args )... );
    std::cout << std::endl;
  }
};

struct Dummy {};

template < typename ...Signatures >
struct Friend;

template < typename ...Signatures >
struct FriendWrapper
{
  using type = typename UpdateSequence<Dummy, FriendImpl, Friend<Signatures...>, Signatures...>::type;
};

template < typename ...Signatures >
struct Friend : FriendWrapper<Signatures...>::type {};

/*
template < typename Base, typename ...Args >
class FriendImpl : Base {

  friend void foo( Args ...args )
  {
    std::cout << "Foo: ";
    tprint( std::forward<Args>( args )... );
    std::cout << std::endl;
  }
};

template < typename ...Signatures >
class Friend;

template <>
class Friend<> {};

template < typename ...Args >
class Friend<void(Args...)> : public FriendImpl<Friend<>, (replace<Placeholder,Friend<void(Args...)>>::template apply_t<Args>)...>
{

};


template < typename ...Args, typename ...Signatures >
class Friend<void(Args...), Signatures...> : public FriendImpl<Friend<Signatures...>, (replace<Placeholder,Friend<void(Args...)>>::template apply_t<Args>)...>
{

};
*/

template < int index >
struct Hint{};

struct Test1 {
  int operator() ( int ) const { std::cout << "Test1::call" << std::endl; return 0; }
  int operator() ( Hint<1>, int ) const { std::cout << "Test1::call Hint<1>" << std::endl; return 0; }
  int operator() ( Hint<2> ) { std::cout << "Test1::call Hint<2>" << std::endl; return 0; }
  void foo() const { std::cout << "Test1::foo" << std::endl; }
};

struct Test2 {
  int operator() ( int ) const { std::cout << "Test2::call" << std::endl; return 0; }
  int operator() ( Hint<1>, int ) const { std::cout << "Test2::call Hint<1>" << std::endl; return 0; }
  int operator() ( Hint<2> ) { std::cout << "Test2::call Hint<2>" << std::endl; return 0; }
  void foo() const { std::cout << "Test2::foo" << std::endl; }
};

RTCI_METHOD_TEMPLATE( Callable, operator() );
RTCI_METHOD_TEMPLATE( Fooable, foo );

using ImmutableObject = rtci_erasure::Immutable<Callable<int(Hint<1>,int) const, int(int) const>,Fooable<void() const>>;
using MutableObject = rtci_erasure::Mutable<sizeof(void*)*2, Callable<int(int) const, int(Hint<1>,int) const, int(Hint<2>)>, Fooable<void()>>;

int main()
{
  {
    
    Friend<void(Placeholder), void(Placeholder, int)> fred{};
    foo( fred );
    foo( fred, 1 );
    //std::cout << get_index( std::index_sequence<2>{}, 0, 1, 2, 3 ) << std::endl;
    //std::cout << forward_at( std::index_sequence<2>{}, std::index_sequence<2>{}, 0, 1, 2, 3 ) << std::endl;
  }
  {
    ImmutableObject object{ Test1{} };
    object( 10 );
    std::cout << rtci_erasure::inspect( object ).name() << std::endl;
    ImmutableObject other = std::move(object);
    std::cout << rtci_erasure::inspect( object ).name() << std::endl;
    other( 5 );
    other( Hint<1>{}, 2 );
    other.foo();
    ImmutableObject empty{};
    std::cout << rtci_erasure::inspect( empty ).name() << std::endl;
  }
  {
    MutableObject object{ Test2{} };
    object( Hint<2>{} );
    object.foo();
    std::cout << rtci_erasure::inspect( object ).name() << std::endl;
    MutableObject other{};
    std::cout << rtci_erasure::inspect( other ).name() << std::endl;
    other = std::move( object );
    std::cout << rtci_erasure::inspect( other ).name() << std::endl;
    MutableObject last{ std::move(other) };
  }
  return 0;
}
