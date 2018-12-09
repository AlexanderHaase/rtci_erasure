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
namespace erasure {

  /// Implementation details
  ///
  namespace detail {

    enum class Operations {
      CopyTo,
      MoveTo
    };

    template < Operations >
    struct Op {};

    /// Virtual Base class, provides virtual destructor.
    ///
    /// @tparam Tag type used to differentiate inheritance hierarchies.
    ///
    template < typename Tag >
    struct VirtualBase
    {
      /*
      virtual void _operation( Op<CopyTo>, void *, size_t, size_t ) const = 0;
      virtual void _operation( Op<MoveTo>, void *, size_t, size_t ) = 0;
      */
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

    /// Interface type combining all mixins.
    ///
    /// TODO: Filter redundant types.
    ///
    template < typename Tag, template <typename> class ...Concepts >
    using Concept = typename Compose<VirtualBase<Tag>, Concepts... >::type;

    /// Inheritance mix-in that stores an object internally.
    ///
    /// @tparam Interface Concept to inherit/implement.
    /// @tparam Object Type to store internally.
    ///
    template < typename Interface, typename Object >
    class InternalObject : public Interface {
     public:    
      template <typename ...Args>
      InternalObject( Args && ...args )
      : object( std::forward<Args>( args )... )
      {}

     protected:
      Object & self() { return object; }
      const Object & self() const { return object; }

     private:
      Object object;
    };

    /// Inheritance mix-in that stores an object externally.
    ///
    /// @tparam Interface Concept to inherit/implement.
    /// @tparam Object Type to store externally.
    ///
    template < typename Interface, typename Object >
    class ExternalObject : public Interface {
     public:    
      template <typename ...Args>
      ExternalObject( Args && ...args )
      : object( std::make_unique<Object>( std::forward<Args>( args )... ) )
      {}

     protected:
      Object & self() { return *object; }
      const Object & self() const { return *object; }

     private:
      std::unique_ptr<Object> object;
    };

    /*
    template < typename Base, typename Internal, typename External >
    struct RegularOps : Base
    {
      void _operation( Op<CopyTo>, void * storage, size_t capacity, size_t alignment ) const final
      {
        if( sizeof(Internal) <= capacity && alignof(Internal) <= alignment )
        {
          new (storage) Internal{ self() };
        }
        else
        {
          new (storage) External{ self(); };
        }
      }
      void _operation( Op<MoveTo>, void * storage, size_t capacity, size_t alignment ) final
      {
        if( sizeof(Internal) <= capacity && alignof(Internal) <= alignment )
        {
          new (storage) Internal{ self() };
        }
        else
        {
          new (storage) External{ self(); };
        }
      }
     protected:
      using Base::self;
    }*/
  } // namespace detail

  template < typename ...Mixins>
  class Immutable : public Mixins::template Container<Immutable<Mixins...>>... {
   public:
    using Tag = Immutable<Mixins...>;
    using Concept = detail::Concept<Tag, Mixins::template Concept...>;

    template < typename Object >
    using Model = typename detail::Compose<detail::InternalObject<Concept, std::decay_t<Object>>, Mixins::template Model...>::type;

    template < typename Object >
    Immutable( Object && other )
    : object( std::make_shared<Model<Object>>( std::forward<Object>( other ) ) )
    {}
   //protected:
    const Concept & self() const { return *object; }
    std::shared_ptr<const Concept> object;
  };

  template < size_t Capacity, typename ...Mixins>
  class Mutable : public Mixins::template Container<Mutable<Capacity, Mixins...>>... {
   public:
    using Tag = Mutable<Capacity, Mixins...>;
    using Concept = detail::Concept<Tag, Mixins::template Concept...>;

    template < typename Object, template <typename, typename> class Storage >
    using Prototype = typename detail::Compose<Storage<Concept, Object>, Mixins::template Model...>::type;

    template < typename Object, bool Fits = (
      alignof(Prototype<Object, detail::InternalObject>) <= alignof(Concept) &&
      sizeof(Prototype<Object, detail::InternalObject>) <= Capacity)>
    struct ModelSelect
    {
      using type = Prototype<Object, detail::InternalObject>;
    };

    template < typename Object >
    struct ModelSelect<Object, false>
    {
      using type = Prototype<Object, detail::ExternalObject>;
    };

    template < typename Object >
    using Model = typename ModelSelect<std::decay_t<Object>>::type;

    template < typename Object >
    Mutable( Object && object )
    {
      static_assert( sizeof(Model<Object>) <= Capacity, "Mutable erasrue capacity too small!" );
      new ( &self() ) Model<Object>{ std::forward<Object>( object ) };
    }

    ~Mutable() { self().~Concept(); }

   //protected:
    std::aligned_storage_t< Capacity, alignof(Concept) > storage;
    const Concept & self() const { return *reinterpret_cast<const Concept*>(&storage); }
    Concept & self() { return *reinterpret_cast<Concept*>(&storage); }
  };
}

#define ERASURE_MIXIN_METHOD_QUALIFIERS( _name_, _method_, _qualifiers_ ) \
  template < typename ...Signatures > \
  struct _name_;  \
  \
  template < typename Return, typename ...Args, typename ...Signatures > \
  struct _name_<Return(Args...) _qualifiers_, Signatures...> { \
  \
    template < typename Derived > \
    struct Container : _name_<Signatures...>::template Container<Derived> \
    { \
      using Parent = typename _name_<Signatures...>::template Container<Derived>; \
      using Parent::_method_;\
      \
      Return _method_( Args ...args ) _qualifiers_ \
      { \
        return static_cast< _qualifiers_ Derived&>(*this).self()._method_( std::forward<Args>( args )... ); \
      } \
    }; \
    \
    template < typename Base > \
    struct Concept : _name_<Signatures...>::template Concept<Base> \
    { \
      using Parent = typename _name_<Signatures...>::template Concept<Base>; \
      using Parent::_method_; \
      \
      virtual Return _method_( Args ...args ) _qualifiers_ = 0; \
    }; \
    \
    template < typename Base > \
    struct Model : _name_<Signatures...>::template Model<Base> \
    { \
      using Parent = typename _name_<Signatures...>::template Model<Base>; \
      using Parent::Parent; \
      \
      Return _method_( Args ...args ) _qualifiers_ final \
      { \
        return self()._method_( std::forward<Args>( args )... ); \
      } \
    \
     protected: \
      using Parent::self; \
    };  \
  };  \
  \
  \
  template < typename ...Args, typename ...Signatures > \
  struct _name_<void(Args...) _qualifiers_, Signatures...> { \
  \
    template < typename Derived > \
    struct Container : _name_<Signatures...>::template Container<Derived> \
    { \
      using Parent = typename _name_<Signatures...>::template Container<Derived>; \
      using Parent::_method_;\
      \
      void _method_( Args ...args ) _qualifiers_ \
      { \
        static_cast< _qualifiers_ Derived&>(*this).self()._method_( std::forward<Args>( args )... ); \
      } \
    }; \
    \
    template < typename Base > \
    struct Concept : _name_<Signatures...>::template Concept<Base> \
    { \
      using Parent = typename _name_<Signatures...>::template Concept<Base>; \
      using Parent::_method_; \
      \
      virtual void _method_( Args ...args ) _qualifiers_ = 0; \
    }; \
    \
    template < typename Base > \
    struct Model : _name_<Signatures...>::template Model<Base> \
    { \
      using Parent = typename _name_<Signatures...>::template Model<Base>; \
      using Parent::Parent; \
      \
      void _method_( Args ...args ) _qualifiers_ final \
      { \
        self()._method_( std::forward<Args>( args )... ); \
      } \
    \
     protected: \
      using Parent::self; \
    };  \
  };  \
  \
  \
  template < typename Return, typename ...Args > \
  struct _name_<Return(Args...) _qualifiers_> { \
  \
    template < typename Derived > \
    struct Container \
    { \
      Return _method_( Args ...args ) _qualifiers_ \
      { \
        return static_cast< _qualifiers_ Derived&>(*this).self()._method_( std::forward<Args>( args )... ); \
      } \
    }; \
    \
    template < typename Base > \
    struct Concept : Base \
    { \
      virtual Return _method_( Args ...args ) _qualifiers_ = 0; \
    }; \
    \
    template < typename Base > \
    struct Model : Base \
    { \
      using Base::Base; \
      \
      Return _method_( Args ...args ) _qualifiers_ final \
      { \
        return self()._method_( std::forward<Args>( args )... ); \
      } \
    \
     protected: \
      using Base::self; \
    };  \
  };  \
  \
  \
  template < typename ...Args > \
  struct _name_<void(Args...) _qualifiers_> { \
    \
    template < typename Derived > \
    struct Container  \
    { \
      void _method_( Args ...args ) _qualifiers_  \
      { \
        static_cast< _qualifiers_ Derived&>(*this).self()._method_( std::forward<Args>( args )... );  \
      } \
    }; \
    \
    template < typename Base >  \
    struct Concept : Base \
    { \
      virtual void _method_( Args ...args ) _qualifiers_ = 0; \
    };  \
    \
    template < typename Base >  \
    struct Model : Base \
    { \
      using Base::Base; \
      \
      void _method_( Args ...args ) _qualifiers_ final  \
      { \
        self()._method_( std::forward<Args>( args )... ); \
      } \
    \
     protected: \
      using Base::self; \
    };  \
  }
// #define ERASURE_MIXIN_METHOD_QUALIFIERS(...)


#define ERASURE_MIXIN_METHOD( _name_, _method_ ) \
  ERASURE_MIXIN_METHOD_QUALIFIERS( _name_, _method_, ); \
  ERASURE_MIXIN_METHOD_QUALIFIERS( _name_, _method_, const )

