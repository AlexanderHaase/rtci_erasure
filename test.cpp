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

template < typename T, T Value >
struct Literal
{
  static constexpr T value = Value;
};

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

  template < typename T >
  using append = List<Args..., T>;

  template < typename T >
  using prepend = List<T, Args...>;

  /*template < template < typename, typename > class Reduce >
  struct reduce
  {
    using type = Reduce<
  };*/
};

template < typename T >
struct reverse;

template < typename Last >
struct reverse<List<Last>> { using type = List<Last>; };

template < typename Current, typename ...Remainder >
struct reverse<List<Current, Remainder...>> 
{
  using type = typename reverse<List<Remainder...>>::template append<Current>;
};

namespace type {

  namespace op
  {
    /// Basic sequence container: can hold arbitrarily many concrete types.
    ///
    template < typename ... >
    struct list {};

    /// Transform a value into a type.
    ///
    template < typename Type, Type Value >
    struct value_type { static constexpr Type value = Value; };

    /// Wrapper for making a list of values as types.
    ///
    template < typename Type, Type ... values >
    using value_list = list< Literal<Type, values>... >;

    /// Specialization for size_t common case
    ///
    template < size_t Value >
    using index_type = value_type<size_t, Value>;

    /// Specialization for size_t common case
    ///
    template < size_t ...Values >
    using index_list = value_list<size_t, Values...>;

    /// Transform a template into a concrete type(limited to templates on concrete types...).
    ///
    template < template < typename ... > class Template >
    struct template_type { template < typename ... Types > using type = Template<Types...>; };

    /// Wrapper for making a list of templates as concrete types.
    ///
    template < template < typename ... > class ...Templates >
    using template_list = list< template_type<Templates>... >;

    /// Generic length operation--implemented via specialization
    ///
    template < typename >
    struct length;

    /// List length specialization
    ///
    template < typename ... Types >
    struct length<list<Types...>> { static constexpr size_t value = sizeof...(Types); };

    /// Generic append operation--implemented via specialization.
    ///
    template < typename Container, typename Element >
    struct append;

    /// Syntatic sugar: append type unwrapping.
    ///
    template < typename Container, typename Element >
    using append_t = typename append<Container, Element>::type;

    /// List append operation.
    ///
    template < typename ... Types, typename Element >
    struct append<list<Types...>, Element> { using type = list<Types..., Element>; };

    /// Generic reverse operation--implemented via specialization.
    ///
    template < typename Container>
    struct reverse;

    /// Syntatic sugar: reverse type unwrapping.
    /// 
    template < typename Container >
    using reverse_t = typename reverse<Container>::type;

    /// List reverse operation(base case: empty list).
    ///
    template <>
    struct reverse<list<>> { using type = list<>; };

    /// List reverse operation(inductive case).
    ///
    template < typename Current, typename ... Remainder >
    struct reverse<list<Current, Remainder...>> { using type = append_t<reverse_t<list<Remainder...>>, Current>; };

    template < typename Container, size_t Begin, size_t End >
    struct slice;

    template < typename Container, size_t Begin, size_t End >
    using slice_t = typename slice<Container, Begin, End >::type;

    template < typename Current, typename ...Remainder >
    struct slice<list<Current, Remainder...>, 0, 1> { using type = list<Current>; };

    template < typename Current, typename ...Remainder, size_t End >
    struct slice<list<Current, Remainder...>, 0, End>
    { 
      using type = reverse_t<append_t<reverse_t<slice_t<list<Remainder...>, 0, (End-1)>>, Current>>; 
    };

    template < typename Current, typename ...Remainder, size_t Begin, size_t End >
    struct slice <list<Current, Remainder...>, Begin, End>
    {
      using type = slice_t<list<Remainder...>, (Begin-1), (End-1)>;
    };

    static_assert( std::is_same<list<int>, slice_t<list<float,int,void>, 1, 2>>::value, "slice!" );

    template < typename Container, template < typename > class Mapper >
    struct map;

    template < typename Container, template < typename > class Mapper >
    using map_t = typename map<Container, Mapper>::type;

    template < typename ... Types, template < typename > class Mapper >
    struct map<list<Types...>, Mapper> { using type = list<Mapper<Types>...>; };

    template < typename Container, template < typename, typename > class Operation, typename Initial >
    struct reduce;

    template < typename Container, template < typename, typename > class Operation, typename Initial >
    using reduce_t = typename reduce<Container, Operation, Initial>::type;

    template < typename Last, template < typename, typename > class Operation, typename Initial >
    struct reduce<list<Last>, Operation, Initial> { using type = Operation<Initial, Last>; };
    
    template < typename Current, typename ...Remainder, template < typename, typename > class Operation, typename Initial >
    struct reduce<list<Current, Remainder...>, Operation, Initial>
    { 
      using type = Operation<reduce_t<list<Remainder...>, Operation, Initial>, Current>;
    };

    template < typename Container, size_t Start = 0 >
    struct enumerate;

    template < typename Container, size_t Start = 0 >
    using enumerate_t = typename enumerate<Container, Start>::type;

    template < size_t Index, typename Type >
    struct enumerated_type
    {
      static constexpr size_t value = Index;
      using type = Type;
    };

    template < size_t Start >
    struct enumerate<list<>, Start> { using type = list<>; };

    template < typename Current, typename ...Remainder, size_t Start >
    struct enumerate<list<Current, Remainder...>, Start>
    {
      using type = reverse_t<append_t<reverse_t<enumerate_t<list<Remainder...>, (Start+1)>>, enumerated_type<Start, Current>>>;
    };

    static_assert( std::is_same<enumerate_t<list<int, float, void>>, list<enumerated_type<0, int>, enumerated_type<1, float>, enumerated_type<2, void>>>::value, "enumerate");

    template < typename Container, typename Index >
    struct get;

    template < typename Container, typename Index >
    using get_t = typename get<Container, Index>::type;

    template < typename Current, typename ...Remainder >
    struct get<list<Current, Remainder...>, index_type<0>> { using type = Current; };

    template < typename Current, typename ...Remainder, size_t Index >
    struct get<list<Current, Remainder...>, index_type<Index>> { using type = get_t<list<Remainder...>, index_type<(Index-1)>>; };

    static_assert( std::is_same<get_t<list<int, float>, index_type<1>>, float>::value, "get" );
  }

  template < typename ... >
  struct conjunction : std::true_type {};

  template < typename Last >
  struct conjunction<Last> : Last
  {
    using type = Last;
  };

  template < typename Current, typename ...Rest >
  struct conjunction< Current, Rest... >
  : std::conditional_t<bool(Current::value), conjunction<Rest...>, Current>
  { 
    using type = std::conditional_t<bool(Current::value), typename conjunction<Rest...>::type, Current>;
  };

  template < typename ...Clauses >
  using conjunction_t = typename conjunction<Clauses...>::type;

  template < typename ... >
  struct disjunction : std::false_type {};

  template < typename Last >
  struct disjunction<Last> : Last
  {
    using type = Last;
  };

  template < typename Current, typename ...Rest >
  struct disjunction<Current, Rest...>
  : std::conditional_t<bool(Current::value), Current, disjunction<Rest...>>
  {
    using type = std::conditional_t<bool(Current::value), Current, typename disjunction<Rest...>::type>;
  };

  template < typename ...Clauses >
  using disjunction_t = typename disjunction<Clauses...>::type;

  template < typename T >
  using negation = std::conditional_t<bool(T::value), std::true_type, std::false_type>;

  template < typename T >
  struct lift
  {
    using type = T;
  };

  template < typename ... >
  using always = std::true_type;

  template < typename ... >
  using never = std::false_type;

  template < typename ... >
  using void_t = void;

  template < typename T >
  struct identity { using type = T; };

  template < template < typename > class Transform, typename T, typename = void >
  struct maybe {};

  template < template < typename > class Transform, typename T >
  struct maybe<Transform, T, void_t<typename T::type>> : Transform<typename T::type> {};

  namespace modifier
  {

    #define RTCI_ERASURE_DEFINE_MODIFIER( _NAME_, _ADD_, _REMOVE_, _PRESENT_ )  \
      struct _NAME_                                                             \
      {                                                                         \
        template < typename U > using apply  = _ADD_<U>;                        \
        template < typename U > using remove = _REMOVE_<U>;                     \
        template < typename U > using present = _PRESENT_<U>;                   \
      }

    RTCI_ERASURE_DEFINE_MODIFIER( none,             lift,                      lift,                  always );
    RTCI_ERASURE_DEFINE_MODIFIER( pointer,          std::add_pointer,          std::remove_pointer,   std::is_pointer );
    RTCI_ERASURE_DEFINE_MODIFIER( constant,         std::add_const,            std::remove_const,     std::is_const );
    RTCI_ERASURE_DEFINE_MODIFIER( volatility,       std::add_volatile,         std::remove_volatile,  std::is_volatile );
    RTCI_ERASURE_DEFINE_MODIFIER( lvalue_reference, std::add_lvalue_reference, std::remove_reference, std::is_lvalue_reference );
    RTCI_ERASURE_DEFINE_MODIFIER( rvalue_reference, std::add_rvalue_reference, std::remove_reference, std::is_rvalue_reference );

    #undef RTCI_ERASURE_DEFINE_MODIFIER

    namespace array
    {
      template < size_t Extent >
      struct extent
      {
        template < typename U > struct apply { using type = U[Extent]; };
        template < typename U > using remove = std::remove_extent<U>;
        template < typename U > using present = conjunction<std::is_array<U>,
          std::conditional<(std::extent<U>::value == Extent), std::true_type, std::false_type>>;
      };

      struct variable
      {
        template < typename U > struct apply { using type = U[]; };
        template < typename U > using remove = std::remove_extent<U>;
        template < typename U > using present = conjunction<std::is_array<U>,
          std::conditional<(std::extent<U>::value == 0), std::true_type, std::false_type>>;
      };

      struct any
      {
        template < typename U > using remove = std::remove_extent<U>;
        template < typename U > using present = std::is_array<U>;
      };
    }

    namespace detail
    {
      template < typename T, typename Modifier, bool = Modifier::template present<T>::value >
      struct detect_if_impl : std::false_type {};

      template < typename T, typename Modifier >
      struct detect_if_impl<T, Modifier, true> : std::true_type { using type = Modifier; };

      template < typename T, size_t Extent >
      struct detect_if_impl<T[Extent], array::any, true> : std::true_type { using type = array::extent<Extent>; };

      template < typename T >
      struct detect_if_impl<T[], array::any, true> : std::true_type { using type = array::variable; };
    }

    template < typename T, typename Modifier >
    using detect_if = detail::detect_if_impl<T, Modifier>;

    template < typename T, typename Modifier >
    using detect_if_t = typename detect_if<T, Modifier>::type;

    template < typename T, typename ...Modifiers >
    using detect_any = disjunction_t<detect_if<T, Modifiers>...>;

    template < typename T, typename ...Modifiers >
    using detect_any_t = typename detect_any<T, Modifiers...>::type;

    template < typename T >
    using detect = detect_any<T, rvalue_reference, lvalue_reference, array::any, volatility, constant, pointer, none >;

    template < typename T>
    using detect_t = typename detect<T>::type;

    static_assert(detect_if<int, none>::value, "none check");
    static_assert(detect_any<int, array::any, none>::value, "none check" );
    static_assert(std::is_same<detect<int>::type, none>::value, "none check");
    static_assert(std::is_same<detect<const int>::type, constant>::value, "const check");
    static_assert(std::is_same<detect<const volatile int>::type, volatility>::value, "volatile check");
    static_assert(std::is_same<detect<const volatile int[]>::type, array::variable>::value, "variable array check");
    static_assert(std::is_same<detect<const volatile int[4]>::type, array::extent<4>>::value, "fixed array check");
    static_assert(std::is_same<detect<const volatile int*>::type, pointer>::value, "pointer check");
    static_assert(std::is_same<detect<const volatile int* &>::type, lvalue_reference>::value, "lvalue ref check");
    static_assert(std::is_same<detect<const volatile int* &&>::type, rvalue_reference>::value, "rvalue ref check");

    namespace detail
    {
      template < typename T, typename Modifier = detect_t<T> >
      struct traits_impl
      {
        using modifier = Modifier;
        using type = T;
        using next = traits_impl<typename modifier::template remove<T>::type>;
        using base = typename next::base;
        static constexpr size_t count = next::count + 1;

        template < typename U >
        using rebind = maybe<modifier::template apply, typename next::template rebind<U>>;

        template < typename U >
        using contains = disjunction_t<std::is_same<T, U>, typename next::template contains<U>>;

        template < typename V, typename U >
        using rebind_at = std::conditional_t<std::is_same<T, V>::value, identity<U>,
          maybe<modifier::template apply, typename next::template rebind_at<V, U>>>;
      };

      template < typename T >
      struct traits_impl<T, none>
      {
        using modifier = none;
        using type = T;
        using base = T;
        static constexpr size_t count = 0;

        template < typename U >
        using rebind = identity<U>;

        template < typename U >
        using contains = std::is_same<T, U>;

        template < typename V, typename U >
        using rebind_at = std::enable_if<std::is_same<T, V>::value, U>;
        //struct rebind_at { using type = U; };
      };
    }

    template < typename T >
    using traits = detail::traits_impl<T>;

    static_assert( traits<int * const volatile>::count == 3, "check traits::count" );
    static_assert( traits<int * const volatile>::contains<int *>::value, "check traits::containts" );
    static_assert( std::is_same<traits<int * const volatile>::base, int>::value, "check traits::base" );
    static_assert( std::is_same<traits<int * const volatile>::rebind<float>::type, float * const volatile>::value, "check traits::rebase" );
    static_assert( std::is_same<traits<int * const volatile>::rebind_at<int*, float>::type, float const volatile>::value, "check traits::rebase_at" );
  }

  namespace signature
  {
    namespace detail
    {
      template < bool Condition, size_t Index, size_t ... Prior >
      using append_if = std::conditional_t<Condition, std::index_sequence<Prior..., Index>, std::index_sequence<Prior...>>;

      template < template < typename > class Predicate, size_t Current, size_t ...Prior, typename Last >
      auto index_of( std::index_sequence<Prior...>, List<Last> )
        -> append_if<Predicate<Last>::value, Current, Prior...>;

      template < template < typename > class Predicate, size_t Current, size_t ...Prior, typename First, typename ...Remainder >
      auto index_of( std::index_sequence<Prior...>, List<First, Remainder...> )
        -> decltype( index_of<Predicate, (Current+1)>( append_if<Predicate<First>::value, Current, Prior...>{}, List<Remainder...>{} ));


      template < typename T >
      using match_int = std::is_same<int, T>;

      using base = List<int, int, float>;
      using matches = typename base::template map<match_int>;

      //using check = decltype(index_of<match_int, 0>( std::index_sequence<>{}, List<int, int, float>{} ));
      //static_assert( std::is_same<check, std::index_sequence<0,1>>::value, "ow" );
    }

    template < typename From, typename To >
    struct rebind_spec
    {
      template < typename T >
      using match = typename modifier::traits<T>::template contains<From>;

      template < typename T >
      using pass = negation<match<T>>;

      template < typename T >
      struct apply
      {
        using type = typename std::conditional_t<match<T>::value,
          typename modifier::traits<T>::template rebind_at<From, To>, identity<T>>::type;
      };

      template < typename T >
      using apply_t = typename apply<T>::type;
    };

    template < typename Signature >
    struct traits;

    template < typename Result, typename ...Args >
    struct traits<Result(Args...)>
    {
      using type = Result(Args...);
      using args = List<Args...>;
      using result = Result;
      static constexpr size_t count = sizeof...(Args);

      template < typename From, typename To >
      using rebind = traits< typename rebind_spec<From, To>::template apply_t<Result>( typename rebind_spec<From, To>::template apply_t<Args>... )>;

      template < typename T >
      using match = decltype(detail::index_of<rebind_spec<T,T>::template match>( Literal<size_t, 0>{}, std::index_sequence<>{}, List<Args...>{} ));

      template < typename T >
      using pass = decltype(detail::index_of<rebind_spec<T,T>::template pass>( Literal<size_t, 0>{}, std::index_sequence<>{}, List<Args...>{} ));
    };

    static_assert( std::is_same<traits<int(int*)>::rebind<int*,float>::type, int(float)>::value, "check traits::rebind");
    //static_assert( std::is_same<traits<int(int *, float, int)>::match<int>, std::index_sequence<0, 2>>::value, "check traits::match");

    template < typename Result, typename ...Args >
    struct traits<Result(Args...) const> : traits<Result(Args...)> {};

    template < typename Result, typename ...Args >
    struct traits<Result(Args...) volatile> : traits<Result(Args...)> {};
  }
}
  /*
  /// Labels for incremental type modifications
  ///
  namespace modifier_type
  {
    struct POINTER{};           ///< e.g. T*
    template <size_t N>
    struct ARRAY{};             ///< e.g. T[N]
    struct CONST{};             ///< e.g. const T
    struct LVALUE_REFERENCE{};  ///< e.g. T &
    struct RVALUE_REFERENCE{};  ///< e.g. T &&
    struct VOLATILE{};          ///< e.g. volatile T
    struct NONE{};               ///< e.g. T
  };

  namespace detail
  {
    /// Detect the outermost modifier for a type. Default: none!
    ///
    template < typename T >
    struct modifier;

    template <>
    struct modifier<modifier_type::NONE>
    {
      static constexpr modifier_type value = modifier_type::NONE;
      template < typename U > struct apply  { using type = U; };
      template < typename U > struct remove { using type = U; };
    };

    #define RTCI_ERASURE_DEFINE_MODIFIER( _NAME_, _ADD_, _REMOVE_ )     \
      template<>                                                        \
      struct modifier<modifier_type::_NAME_>                            \
      {                                                                 \
        using type = modifier_type::_ENUM_;                             \
        template < typename U > using apply  = _ADD_<U>;                \
        template < typename U > using remove = _REMOVE_<U>;             \
      }

    RTCI_ERASURE_DEFINE_MODIFIER( POINTER,          std::add_pointer,          std::remove_pointer   );
    RTCI_ERASURE_DEFINE_MODIFIER( CONST,            std::add_const,            std::remove_const     );
    RTCI_ERASURE_DEFINE_MODIFIER( VOLATILE,         std::add_volatile,         std::remove_volatile  );
    RTCI_ERASURE_DEFINE_MODIFIER( LVALUE_REFERENCE, std::add_lvalue_reference, std::remove_reference );
    RTCI_ERASURE_DEFINE_MODIFIER( RVALUE_REFERENCE, std::add_rvalue_reference, std::remove_reference );

    #undef RTCI_ERASURE_DEFINE_MODIFIER

    template < typename, typename = void >
    struct detect_modifier { using type = modifier<NONE>; };

    #define RTCI_ERASURE_DETECT_MODIFIER( _NAME_, _T_NAME_, _CONDITION_ ) \
      template < typename _T_NAME_ >                                      \
      struct detect_modifier<_T_NAME_,                                    \
         std::enable_if_t<(_CONDITION_)>>                                 \
      {                                                                   \
        using type = modifier<_NAME_>;                                    \
      }

    // Induce a strict modifier ordering during detection:
    //
    //  - Reference(L or R value)
    //  - Volatile
    //  - Const
    //  - Pointer
    //  - None(default)
    //
    RTCI_ERASURE_DETECT_MODIFIER( LVALUE_REFERENCE, T, std::is_lvalue_reference<T>::value );
    RTCI_ERASURE_DETECT_MODIFIER( RVALUE_REFERENCE, T, std::is_rvalue_reference<T>::value );
    RTCI_ERASURE_DETECT_MODIFIER( VOLATILE,         T, std::is_volatile<T>::value         );
    RTCI_ERASURE_DETECT_MODIFIER( CONST,            T, std::is_const<T>::value            && 
                                                       !std::is_volatile<T>::value        );
    RTCI_ERASURE_DETECT_MODIFIER( POINTER,          T, std::is_pointer<T>::value          &&
                                                       !std::is_const<T>::value           && 
                                                       !std::is_volatile<T>::value        );

    #undef RTCI_ERASURE_DETECT_MODIFIER

    template <typename T>
    using detect_modifier_t = typename detect_modifier<T>::type;

    static_assert( !std::is_pointer<int *&>() && std::is_reference<int *&>(), "ow" );
    static_assert( modifier<modifier_type::CONST>::value == modifier_type::CONST, "check identity" );

    static_assert( detect_modifier_t<volatile const int *&>::value == modifier_type::LVALUE_REFERENCE, "check modifier ordering: LVALUE_REFERENCE" );
    static_assert( detect_modifier_t<volatile const int *&&>::value == modifier_type::RVALUE_REFERENCE, "check modifier ordering: RVALUE_REFERENCE" );
    static_assert( detect_modifier_t<int * volatile const>::value == modifier_type::VOLATILE, "check modifier ordering: VOLATILE" );
    static_assert( detect_modifier_t<int * const>::value == modifier_type::CONST, "check modifier ordering: CONST" );
    static_assert( detect_modifier_t<int * >::value == modifier_type::POINTER, "check modifier ordering: POINTER" );
    static_assert( detect_modifier_t<volatile const int>::value == modifier_type::VOLATILE, "check modifier ordering: VOLATILE" );
    static_assert( detect_modifier_t<const int>::value == modifier_type::CONST, "check modifier ordering: CONST" );
    static_assert( detect_modifier_t<int>::value == modifier_type::NONE, "check modifier ordering: NONE" );
  }
}

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


template < size_t ...Prior, size_t Last>
auto iterate( std::index_sequence<Prior...>, std::index_sequence<Last> )
{
  std::cout << Last << std::endl;
  return std::index_sequence<Prior..., Last>{};
}

template < size_t ...Prior, size_t Current, size_t ...Remainder >
auto iterate( std::index_sequence<Prior...>, std::index_sequence<Current, Remainder...> )
-> decltype( iterate( std::index_sequence<Prior..., Current>{}, std::index_sequence<Remainder...>{} ))
{
  std::cout << Current << ", ";
  return iterate( std::index_sequence<Prior..., Current>{}, std::index_sequence<Remainder...>{} );
}

template < typename First, typename ...Args >
auto get_index( Literal<size_t, 0>, First && first, Args && ... )
{
  return first;
}

template < size_t Index, typename First, typename ...Args >
auto get_index( Literal<size_t, Index>, First &&, Args && ... args )
  -> decltype(get_index( Literal<size_t, Index-1>{}, std::forward<Args>(args)... ))
{
  return get_index( Literal<size_t, Index-1>{}, std::forward<Args>(args)... );
}

template < size_t Select, typename A, typename ...Args >
auto forward_at( Literal<size_t, Select>, Literal<size_t, Select>, A && a, Args && ... )
  -> A
{
  return a;
}

template < size_t Select, size_t Index, typename A, typename ...Args >
auto forward_at( Literal<size_t, Select>, Literal<size_t, Index>, A &&, Args && ...args )
  -> std::enable_if_t<(Select < Index), decltype(get_index(Literal<size_t, Index>{}, std::forward<Args>(args)...))>
{
  return get_index(Literal<size_t, Index>{}, std::forward<Args>(args)...);
}

template < size_t Select, size_t Index, typename A, typename ...Args >
auto forward_at( std::index_sequence<Select>, std::index_sequence<Index>, A &&, Args && ...args )
  -> std::enable_if_t<(Select > Index), decltype(get_index(Literal<size_t, Index+1>{}, std::forward<Args>(args)...))>
{
  return get_index(Literal<size_t, Index+1>{}, std::forward<Args>(args)...);
}

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
    std::cout << typeid(type::modifier::traits<int *>::rebind_at<int*, float>::type).name() << std::endl;
    std::cout << typeid(float).name() << std::endl;
    std::cout << typeid(int(int)).name() << std::endl;
    std::cout << typeid(type::signature::traits<int(int)>::type).name() << std::endl;
    std::cout << typeid(type::signature::traits<int(int)>::rebind<int,float>::type).name() << std::endl;
    std::cout << typeid(float(float)).name() << std::endl;
    std::cout << std::is_same<type::signature::traits<int(int)>::rebind<int,float>::type, float(float)>::value << std::endl;
    /*Friend<void(Placeholder), void(Placeholder, int)> fred{};
    foo( fred );
    foo( fred, 1 );
    //iterate( std::index_sequence<>{}, std::index_sequence<0, 1, 2, 3>{} );
    std::cout << get_index( Literal<size_t, 2>{}, 0, 1, 2, 3 ) << std::endl;
    std::cout << forward_at( Literal<size_t, 2>{}, Literal<size_t, 2>{}, 0, 1, 2, 3 ) << std::endl;
    */
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
