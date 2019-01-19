#include <type_traits>
#include <memory>

namespace rtci {

  namespace type
  {
    namespace op
    {
      struct empty {};

      template < typename Type >
      struct identity { using type = Type; };

      template < typename Type >
      using unwrap = typename Type::type;

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

      namespace detail
      {
        template < typename Type, typename = void >
        struct make_iterable;

        template < typename Type >
        struct make_iterable<Type, std::enable_if_t<is_iterator<Type>::value>> { using type = Type; };

        template < typename Type >
        struct make_iterable<Type, std::enable_if_t<has_op<begin, Type>::value>> : begin<Type> {};
      }

      template < typename Type >
      struct iterate : detail::make_iterable<Type> {};

      template < typename Type >
      using iterate_t = typename iterate<Type>::type;

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
      template < typename Iterator, int Amount >
      struct seek
      { 
        using step = unwrap<std::conditional_t<(Amount < 0), maybe<prev, Iterator>, maybe<next, Iterator>>>;
        static constexpr int increment = (Amount < 0 ? -1 : 1);
        using type = unwrap<seek<step, (Amount - increment)>>;
      };

      template < typename Iterator>
      struct seek<Iterator, 0> { using type = Iterator; };

      template < typename Iterator, int Amount >
      using seek_t = typename seek<Iterator, Amount>::type;

      /// Reverse implementation
      ///
      struct reverse_tag {};

      template < typename Iterator >
      struct reverse { using type = iterator<reverse_tag, iterate_t<Iterator>, void>; };

      template < typename Iterator >
      using reverse_t = typename reverse<Iterator>::type;

      template < typename Iterator >
      struct next<iterator<reverse_tag, Iterator, std::enable_if_t<has_op<prev, Iterator>::value>>>
      { using type = iterator<reverse_tag, prev_t<Iterator>, void>; };

      template < typename Iterator >
      struct prev<iterator<reverse_tag, Iterator, std::enable_if_t<has_op<next, Iterator>::value>>>
      { using type = iterator<reverse_tag, next_t<Iterator>, void>; };

      template < typename Iterator >
      struct eval<iterator<reverse_tag, Iterator, std::enable_if_t<has_op<eval, Iterator>::value>>>
      { using type = eval_t<Iterator>; };

      /// Map implementation
      ///
      struct map_tag {};

      template < template < typename > class Op, typename Iterator >
      struct map { using type = iterator< map_tag, template_type<Op>, iterate_t<Iterator>, void >; };

      template < template < typename > class Op, typename Iterator >
      using map_t = typename map<Op, Iterator>::type;

      template < template < typename > class Op, typename Iterator >
      struct next<iterator<map_tag, template_type<Op>, Iterator, std::enable_if_t<has_op<next, Iterator>::value>>>
      { using type = iterator<map_tag, template_type<Op>, next_t<Iterator>, void>; };

      template < template < typename > class Op, typename Iterator >
      struct prev<iterator<map_tag, template_type<Op>, Iterator, std::enable_if_t<has_op<prev, Iterator>::value>>>
      { using type = iterator<map_tag, template_type<Op>, prev_t<Iterator>, void>; };

      template < template < typename > class Op, typename Iterator >
      struct eval<iterator<map_tag, template_type<Op>, Iterator, std::enable_if_t<has_op<eval, Iterator>::value>>>
      { using type = Op<eval_t<Iterator>>; };

      /// Repeat implementation
      ///
      template < template < typename > class Op, typename Init, size_t N >
      struct repeat
      {
        template < typename Type >
        using recurse = repeat<Op, Op<Type>, (N-1)>;
        using type = unwrap<std::conditional_t<(N==0), identity<Init>, maybe<recurse, Init>>>;
      };

      template < template < typename > class Op, typename Init, size_t N >
      using repeat_t = unwrap<repeat<Op, Init, N>>;

      template < template < typename > class Op, typename Init, template < typename > class Predicate >
      struct repeat_until
      {
          template < typename Type >
          using recurse = repeat_until<Op, Op<Type>, Predicate>;
          using type = typename std::conditional_t< Predicate<Init>::value, identity<Init>, maybe<recurse, Init>>::type;
      };

      template < template < typename > class Op, typename Init, template < typename > class Predicate >
      using repeat_until_t = unwrap<repeat_until<Op, Init, Predicate>>;

      /// Filter implementation
      ///
      struct filter_tag {};

      template < template < typename > class Op, typename Iterator >
      struct filter { using type = iterator< filter_tag, template_type<Op>, iterate_t<Iterator>, void, void >; };

      template < template < typename > class Op, typename Iterator >
      using filter_t = typename filter<Op, Iterator>::type;


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
      static_assert( std::is_same<repeat_t<next_t, begin_t<list<int, int>>, 2>, end_t<list<int, int>>>::value, "check repeat" );
      //static_assert( std::is_same<repeat_until_t<next_t, begin_t<list<int, int>>,
      static_assert( std::is_same<seek_t<begin_t<list<int, int>>,2>, end_t<list<int, int>>>::value, "check seek forward" );
      static_assert( std::is_same<seek_t<end_t<list<int, int>>,-2>, begin_t<list<int, int>>>::value, "check seek reverse" );
      static_assert( length<list<int, int>>::value == 2, "check length" );
      static_assert( length<reverse_t<end_t<list<int, int>>>>::value == 2, "check reverse iteration" );
      static_assert( eval_t<map_t<is_iterator, list<iterator<>>>>::value, "check map eval" );
    }
  }
}

int main() {}
