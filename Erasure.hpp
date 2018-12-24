#pragma once
#include <memory>
#include <type_traits>

/// A generic approach to the runtime-concept idiom.
///
/// The runtime-concept idiom provides a pattern for run-time polymorphism that
/// isolates polymorphism into an implementation detail. The runtime-concept
/// idiom uses a pimpl pattern style to virtualize arbitrary classes with
/// templates. The pimpl pattern is captured into a "concept" virtual base
/// class that is teathered to the containing object. A "model" template class
/// implements the concept for a specific type.
///
/// Call path:
///   Container::method() -> Concept::method() -> Model::method()
///
/// The runtime-concept idiom leaves crafting the container, concept, and model
/// to the developer. However, this is in fact generic boilerplate that
/// declares methods with a specific signature:
///
///   Container::method_1() -> Concept::method_1() -> Model::method_1()
///   Container::method_2() -> Concept::method_2() -> Model::method_2()
///   ...
///   Container::method_n() -> Concept::method_n() -> Model::method_n()
///   
/// The mixin skills idiom provides a pattern for defining these methods.
/// Macros can be used to propagate the method signature to each call site when
/// defining mixins. This reduces using the runtime-concept idiom to defining
/// a set of mixins.
///
/// There is one branch-point in the runtime-concept idiom pattern: mutability.
/// Specifically, immutable objects can be optimized by using shared pointers
/// to a const concept. Mutable erasures can offer a small object optimization.
///
/// This header-only library provides mutable and immutable template classes
/// for the runtime-concept idiom, and a macro for the boilerplate of making
/// mixins. The implementations are suitable for use with regular types.
///
namespace rtci_erasure {

  /// Placeholder structure for free function signatures.
  ///
  struct Placeholder {};

  /// Implementation detail
  ///
  namespace detail {

    namespace hint {

      struct Self{};

      template < typename ... >
      struct Method {};

      template < typename ... >
      struct Function {};
    }

    /// Virtual Base class, provides virtual destructor.
    ///
    /// @tparam Tag type used to differentiate inheritance hierarchies.
    ///
    template < typename Tag >
    struct VirtualBase
    {
      inline void invoke_method( hint::Method<Tag> ) const {}
      virtual ~VirtualBase() {}
    };

    /// Template composition
    ///
    /// @tparam Initial concrete type to use to start recursive composition.
    /// @tparam Next template type to use for composition.
    /// @tparam Remainder of template types to compose.
    ///
    template < typename Initial, template <typename> class Next, template <typename> class ...Remainder >
    struct Compose
    {
      using type = typename Compose<Next<Initial>, Remainder...>::type;
    };

    /// Specialization to terminate recursion.
    ///
    /// @tparam Initial concrete type to use to start recursive composition.
    /// @tparam Last template type to use for composition.
    ///
    template < typename Initial, template <typename> class Last >
    struct Compose<Initial, Last>
    {
      using type = Last<Initial>;
    };

    namespace storage {
      enum class Method
      {
        Internal,
        External
      };

      template < typename Base, typename Object, Method Method >
      class Storage;

      /// Inheritance mix-in that stores an object externally.
      ///
      /// @tparam Base Class to inherit/implement.
      /// @tparam Object Type to store externally.
      ///
      template < typename Base, typename Object >
      class Storage<Base, Object, Method::External> : public Base {
       public:
        using Base::invoke_method;

        template < typename ...Args >
        Storage( Args && ...args )
        : object( std::make_unique<Object>( std::forward<Args>( args )... ) )
        {}

       protected:
        Object & invoke_method( hint::Self ) { return *object; }
        const Object & invoke_method( hint::Self ) const { return *object; }

       private:
        std::unique_ptr<Object> object;
      };

      /// Inheritance mix-in that stores an object internally.
      ///
      /// @tparam Base Class to inherit/implement.
      /// @tparam Object Type to store internally.
      ///
      template < typename Base, typename Object >
      class Storage<Base, Object, Method::Internal> : public Base {
       public:
        using Base::invoke_method;

        template < typename ...Args >
        Storage( Args && ...args )
        : object( std::forward<Args>( args )... )
        {}

       protected:
        Object & invoke_method( hint::Self ) { return object; }
        const Object & invoke_method( hint::Self ) const { return object; }

       private:
        Object object;
      };    

      template < typename Base, typename Object, size_t Capacity >
      struct Select {
        using Internal = Storage<Base, Object, Method::Internal>;
        using External = Storage<Base, Object, Method::External>;
        using type = std::conditional_t<(sizeof(Internal) <= Capacity), Internal, External>;
        static_assert( sizeof(type) <= Capacity, "Capacity too small for any storage method!" );
      };
    }

    /*
    template <typename Target, typename Replace, typename Check>
    struct replace_with
    {
      using type = std::conditional_t<std::is_same_v<Original, Check>, Replace, Check>;
    };

    template <typename Target, typename Replace, typename Check>
    using replace_with_t = typename replace_with<Target, Replace, Check>::type;

    template <typename Target, typename Replace, typename Signature>
    struct update_signature;

    template <typename Target, typename Replace, typename Return, typename ...Args>
    struct update_signature<Original, Replace, Return(Args...)>
    {
      using type = replace_with_t<Target, Replace, Return>( replace_with_t<Target, Replace, Args>...);
    };

    template <typename Target, typename Replace, typename Signature>
    using update_signature_t = typename update_signature<Target, Replace, Signature>::type;
    */

    namespace base {
      
      template < typename ...Mixins >
      class Immutable {
       public:
        template < typename Object >
        Immutable( Object && object )
        : object_( std::make_shared<Model<Object>>( std::forward<Object>( object ) ) )
        {}

       protected:
        using Tag = Immutable<Mixins...>;
        using Concept = typename Compose< VirtualBase<Tag>, Mixins::template Concept...>::type;

        template < typename Object >
        using Storage = storage::Storage<Concept, std::decay_t<Object>, storage::Method::Internal>;

        template < typename Object >
        using Model = typename Compose< Storage<Object>, Mixins::template Model...>::type;

        const Concept & invoke_method( hint::Self ) const { return *object_; }
       private:
        std::shared_ptr<const Concept> object_;
      };

      template < size_t Capacity, typename ...Mixins >
      class Mutable {
       public:
        template < typename Object >
        Mutable( Object && object )
        {
          new (&storage_) Model<Object>{ std::forward<Object>( object ) };
        }

        ~Mutable()
        {
          reinterpret_cast<Concept*>( &storage_ )->~Concept();
        }

        Mutable( const Mutable & other ) = delete;
        Mutable( Mutable && other ) = delete;

        Mutable & operator = ( const Mutable & other ) = delete;
        Mutable & operator = ( Mutable && other ) = delete;

       protected:
        using Tag = Immutable<Mixins...>;
        using Concept = typename Compose< VirtualBase<Tag>, Mixins::template Concept...>::type;

        template < typename Object >
        using Storage = typename storage::Select<Concept, std::decay_t<Object>, Capacity>::type;

        template < typename Object >
        using Model = typename Compose< Storage<Object>, Mixins::template Model...>::type;

        const Concept & invoke_method( hint::Self ) const { return *reinterpret_cast<const Concept*>( &storage_ ); }
        Concept & invoke_method( hint::Self ) { return *reinterpret_cast<Concept*>( &storage_ ); }
       private:
        std::aligned_storage_t< Capacity, alignof(Concept) > storage_;
      };
    }
  } // namespace detail

  template < typename ...Mixins >
  using Immutable = typename detail::Compose< detail::base::Immutable<Mixins...>, Mixins::template Container... >::type;

  template < size_t Capacity, typename ...Mixins >
  using Mutable = typename detail::Compose< detail::base::Mutable<Capacity, Mixins...>, Mixins::template Container... >::type;
}

#define RTCI_MIXIN_METHOD_QUALIFIERS( _name_, _method_, _qualifiers_ ) \
  template < typename ...Signatures > \
  struct _name_;  \
  \
  template < typename Return, typename ...Args, typename ...Signatures > \
  struct _name_<Return(Args...) _qualifiers_, Signatures...> \
  { \
    template < typename Base > \
    struct Container : _name_<Signatures...>::template Container<Base> \
    { \
      using Parent = typename _name_<Signatures...>::template Container<Base>; \
      using Parent::Parent; \
      using Parent::invoke_method; \
      using Parent::_method_; \
      \
      Return _method_( Args ...args ) _qualifiers_ \
      { \
        _qualifiers_ auto & self = invoke_method( ::rtci_erasure::detail::hint::Self{} ); \
        return self.invoke_method( ::rtci_erasure::detail::hint::Method<_name_>{}, std::forward<Args>( args )... ); \
      } \
    }; \
    \
    template < typename Base > \
    struct Concept : _name_<Signatures...>::template Concept<Base> \
    { \
      using Parent = typename _name_<Signatures...>::template Concept<Base>; \
      using Parent::invoke_method; \
      \
      virtual Return invoke_method( ::rtci_erasure::detail::hint::Method<_name_>, Args ...args ) _qualifiers_ = 0; \
    }; \
    \
    template < typename Base > \
    struct Model : _name_<Signatures...>::template Model<Base> \
    { \
      using Parent = typename _name_<Signatures...>::template Model<Base>; \
      using Parent::Parent; \
      using Parent::invoke_method; \
      \
      Return invoke_method( ::rtci_erasure::detail::hint::Method<_name_>, Args ...args ) _qualifiers_ final \
      { \
        _qualifiers_ auto & self = invoke_method( ::rtci_erasure::detail::hint::Self{} ); \
        return self._method_( std::forward<Args>( args )... ); \
      } \
    };  \
  };  \
  \
  \
  template < typename ...Args, typename ...Signatures > \
  struct _name_<void(Args...) _qualifiers_, Signatures...> \
  { \
    template < typename Base > \
    struct Container : _name_<Signatures...>::template Container<Base> \
    { \
      using Parent = typename _name_<Signatures...>::template Container<Base>; \
      using Parent::Parent; \
      using Parent::invoke_method; \
      using Parent::_method_; \
      \
      void _method_( Args ...args ) _qualifiers_ \
      { \
        _qualifiers_ auto & self = invoke_method( ::rtci_erasure::detail::hint::Self{} ); \
        self.invoke_method( ::rtci_erasure::detail::hint::Method<_name_>{}, std::forward<Args>( args )... ); \
      } \
    }; \
    \
    template < typename Base > \
    struct Concept : _name_<Signatures...>::template Concept<Base> \
    { \
      using Parent = typename _name_<Signatures...>::template Concept<Base>; \
      using Parent::Parent; \
      using Parent::invoke_method; \
      \
      virtual void invoke_method( ::rtci_erasure::detail::hint::Method<_name_>, Args ...args ) _qualifiers_ = 0; \
    }; \
    \
    template < typename Base > \
    struct Model : _name_<Signatures...>::template Model<Base> \
    { \
      using Parent = typename _name_<Signatures...>::template Model<Base>; \
      using Parent::Parent; \
      using Parent::invoke_method; \
      \
      void invoke_method( ::rtci_erasure::detail::hint::Method<_name_>, Args ...args ) _qualifiers_ final \
      { \
        _qualifiers_ auto & self = invoke_method( ::rtci_erasure::detail::hint::Self{} ); \
        self._method_( std::forward<Args>( args )... ); \
      } \
    };  \
  };  \
  \
  \
  template < typename Return, typename ...Args > \
  struct _name_<Return(Args...) _qualifiers_> \
  { \
    template < typename Base > \
    struct Container : Base \
    { \
      using Parent = Base; \
      using Parent::Parent; \
      using Parent::invoke_method; \
      \
      Return _method_( Args ...args ) _qualifiers_ \
      { \
        _qualifiers_ auto & self = invoke_method( ::rtci_erasure::detail::hint::Self{} ); \
        return self.invoke_method( ::rtci_erasure::detail::hint::Method<_name_>{}, std::forward<Args>( args )... ); \
      } \
    }; \
    \
    template < typename Base > \
    struct Concept : Base \
    { \
      using Parent = Base; \
      using Parent::invoke_method; \
      virtual Return invoke_method( ::rtci_erasure::detail::hint::Method<_name_>, Args ...args ) _qualifiers_ = 0; \
    }; \
    \
    template < typename Base > \
    struct Model : Base \
    { \
      using Parent = Base; \
      using Parent::Parent; \
      using Parent::invoke_method; \
      \
      Return invoke_method( ::rtci_erasure::detail::hint::Method<_name_>, Args ...args ) _qualifiers_ final \
      { \
        _qualifiers_ auto & self = invoke_method( ::rtci_erasure::detail::hint::Self{} ); \
        return self._method_( std::forward<Args>( args )... ); \
      } \
    };  \
  };  \
  \
  \
  template < typename ...Args > \
  struct _name_<void(Args...) _qualifiers_> \
  { \
    template < typename Base > \
    struct Container : Base \
    { \
      using Parent = Base; \
      using Parent::Parent; \
      using Parent::invoke_method; \
      \
      void _method_( Args ...args ) _qualifiers_ \
      { \
        _qualifiers_ auto & self = invoke_method( ::rtci_erasure::detail::hint::Self{} ); \
        self.invoke_method( ::rtci_erasure::detail::hint::Method<_name_>{}, std::forward<Args>( args )... ); \
      } \
    }; \
    \
    template < typename Base > \
    struct Concept : Base \
    { \
      using Parent = Base; \
      using Parent::invoke_method; \
      virtual void invoke_method( ::rtci_erasure::detail::hint::Method<_name_>, Args ...args ) _qualifiers_ = 0; \
    }; \
    \
    template < typename Base > \
    struct Model : Base \
    { \
      using Parent = Base; \
      using Parent::Parent; \
      using Parent::invoke_method; \
      \
      void invoke_method( ::rtci_erasure::detail::hint::Method<_name_>, Args ...args ) _qualifiers_ final \
      { \
        _qualifiers_ auto & self = invoke_method( ::rtci_erasure::detail::hint::Self{} ); \
        self._method_( std::forward<Args>( args )... ); \
      } \
    };  \
  }
// #define RTCI_MIXIN_METHOD_QUALIFIERS(...)


#define RTCI_MIXIN_METHOD( _name_, _method_ ) \
  RTCI_MIXIN_METHOD_QUALIFIERS( _name_, _method_, ); \
  RTCI_MIXIN_METHOD_QUALIFIERS( _name_, _method_, const )

