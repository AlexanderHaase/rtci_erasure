#include <type_traits>
#include <memory>

namespace rtci {

  namespace type
  {
    namespace op
    {
      struct empty {};

      namespace detail
      {
        /// More flexible variant of void_t
        ///
        template < typename Type, typename ... >
        using if_expr = Type;

        template < template < typename > class Op, typename Type, typename = void >
        struct check_op : std::false_type {};

        template < template < typename > class Op, typename Type>
        struct check_op<Op, Type, if_expr<void, typename Op<Type>::type>> : std::true_type {};

        template < template < typename > class Op, typename Type, typename Failure, typename = void >
        struct try_op : Failure {};

        template < template < typename > class Op, typename Type, typename Failure>
        struct try_op<Op, Type, Failure, if_expr<void, typename Op<Type>::type>> : Op<Type> {};
      }

      template < template < typename > class Op, typename Type >
      struct has_op : detail::check_op<Op, Type> {};

      template < template < typename > class Op, typename Type, typename Failure = empty >
      struct maybe : detail::try_op<Op, Type, Failure> {};

      template < template < typename > class Current, template < typename > class ...Rest >
      struct compose
      {
        template < typename Type >
        using apply = Current<typename compose<Rest...>::template apply<Type>>;
      };

      template < template < typename > class Current >
      struct compose<Current>
      {
        template < typename Type >
        using apply = Current<Type>;
      };

      #define RTCI_TYPE_OP_DECLARE( _NAME_, _TPARAM_ ) \
      template < typename _TPARAM_ >  \
      struct _NAME_;  \
      \
      template < typename _TPARAM_ >  \
      using _NAME_ ## _t = typename _NAME_<_TPARAM_>::type

      RTCI_TYPE_OP_DECLARE( eval, Expression );
      RTCI_TYPE_OP_DECLARE( begin, Collection );
      RTCI_TYPE_OP_DECLARE( end, Collection );
      RTCI_TYPE_OP_DECLARE( next, Iterator );
      RTCI_TYPE_OP_DECLARE( prev, Iterator );

      #undef RTCI_TYPE_OP_DECLARE

      template < typename Type, Type Value >
      struct value_type { static constexpr Type value = Value; };

      template < size_t Value >
      using index_type = value_type<size_t, Value>;

      template < template < typename ... > class Template >
      struct template_type { template < typename ...Types > using apply = Template<Types...>; };

      /// Provide a standard iterator type.
      ///
      template < typename ... >
      struct iterator {};

      /// Detect type based on template specialization (allows user defined iterators)
      ///
      template < typename >
      struct is_iterator : std::false_type {};

      template < typename ... Types>
      struct is_iterator<iterator<Types...>> : std::true_type {};

      template < typename Iterator, int Amount >
      struct seek
      { 
        using step = typename std::conditional_t<(Amount < 0), maybe<prev, Iterator>, maybe<next, Iterator>>::type;
        static constexpr int increment = (Amount < 0 ? -1 : 1);
        using type = typename seek<step, (Amount - increment)>::type;
      };

      namespace detail
      {
        /// Detect length by specialization (using sfinae to detect/enable on traits).
        ///
        template < typename Type, typename = void >
        struct detect_length;

        /// Base case: iterator with no next operation
        ///
        template < typename Iterator >
        struct detect_length<Iterator,
          std::enable_if_t<(is_iterator<Iterator>::value && !has_op<next, Iterator>::value)>>
        {
          static constexpr size_t value = 0;
        };

        /// Inductive case: add one to the length of the next iterator.
        ///
        template < typename Iterator >
        struct detect_length<Iterator,
          std::enable_if_t<(is_iterator<Iterator>::value && has_op<next, Iterator>::value)>>
        {
          static constexpr size_t value = detect_length<next_t<Iterator>>::value + 1;
        };

        /// Monkey type: if it can become an iterator, check it's length via iteration.
        ///
        template < typename Collection >
        struct detect_length<Collection, std::enable_if_t<has_op<begin, Collection>::value>>
        {
          static constexpr size_t value = detect_length<begin_t<Collection>>::value;
        };
      }

      /// Length operation on iterators or collections
      ///
      template < typename Iterable >
      struct length : detail::detect_length<Iterable> {};

      /// Seek operation on iterators
      ///
      template < typename Iterator>
      struct seek<Iterator, 0> { using type = Iterator; };

      template < typename Iterator, int Amount >
      using seek_t = typename seek<Iterator, Amount>::type;

      /// Reverse implementation
      ///
      template < typename Iterator >
      struct reverse;

      /// Work around for clang template limitations
      ///
      using reverse_type = template_type<reverse>;

      template < typename Iterator >
      struct reverse { using type = iterator<reverse_type, Iterator, void>; };

      template < typename Iterator >
      using reverse_t = typename reverse<Iterator>::type;

      template < typename Iterator >
      struct next<iterator<template_type<reverse>, Iterator, std::enable_if_t<has_op<prev, Iterator>::value>>>
      { using type = iterator<template_type<reverse>, prev_t<Iterator>, void>; };

      template < typename Iterator >
      struct prev<iterator<template_type<reverse>, Iterator, std::enable_if_t<has_op<next, Iterator>::value>>>
      { using type = iterator<template_type<reverse>, next_t<Iterator>, void>; };

      template < typename Iterator >
      struct eval<iterator<template_type<reverse>, Iterator, std::enable_if_t<has_op<eval, Iterator>::value>>>
      { using type = eval_t<Iterator>; };

      /// List implementation
      ///
      template < typename ... >
      struct list {};

      template < typename ...Types >
      struct begin<list<Types...>> { using type = iterator<list<>, list<Types...>>; };

      template < typename ...Types >
      struct end<list<Types...>> { using type = iterator<list<Types...>, list<>>; };

      template < typename Current, typename ...Rest, typename ...Prior >
      struct next<iterator<list<Prior...>, list<Current, Rest...>>> { using type = iterator<list<Current, Prior...>, list<Rest...>>; };

      template < typename Current, typename ...Rest, typename ...Prior >
      struct prev<iterator<list<Current, Prior...>, list<Rest...>>> { using type = iterator<list<Prior...>, list<Current, Rest...>>; };

      template < typename Current, typename ...Rest, typename ...Prior >
      struct eval<iterator<list<Prior...>, list<Current, Rest...>>> { using type = Current; };

      static_assert( std::is_same<begin_t<list<>>, end_t<list<>>>::value, "check empty list" );
      static_assert( std::is_same<eval_t<begin_t<list<int>>>, int>::value, "check list iterator eval" );
      static_assert( std::is_same<next_t<begin_t<list<int>>>, end_t<list<int>>>::value, "check list next" );
      static_assert( std::is_same<prev_t<end_t<list<int>>>, begin_t<list<int>>>::value, "check list prev" );
      static_assert( !has_op<eval, begin_t<list<>>>::value, "check has_op<eval, ...>" );
      static_assert( !has_op<next, begin_t<list<>>>::value, "check has_op<eval, ...>" );
      static_assert( !has_op<prev, begin_t<list<>>>::value, "check has_op<eval, ...>" );
      static_assert( has_op<eval, begin_t<list<int>>>::value, "check has_op<eval, ...>" );
      static_assert( has_op<next, begin_t<list<int>>>::value, "check has_op<eval, ...>" );
      static_assert( has_op<prev, end_t<list<int>>>::value, "check has_op<eval, ...>" );
      static_assert( std::is_same<seek_t<begin_t<list<int, int>>,2>, end_t<list<int, int>>>::value, "check seek forward" );
      static_assert( std::is_same<seek_t<end_t<list<int, int>>,-2>, begin_t<list<int, int>>>::value, "check seek reverse" );
      static_assert( length<list<int, int>>::value == 2, "check length" );
      static_assert( std::is_same<eval_t<next_t<next_t<reverse_t<end_t<list<int, int>>>>>>, int>::value, "check reverse eval" );
      static_assert( length<reverse_t<end_t<list<int, int>>>>::value == 2, "check reverse iteration" );
    }
  }
}

int main() {}
