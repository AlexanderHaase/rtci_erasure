#include "Erasure.hpp"
#include <iostream>

template < int index >
struct Hint{};

RTCI_METHOD_TEMPLATE( Callable, operator() );
RTCI_METHOD_TEMPLATE( Fooable, foo );

using ImmutableObject = rtci_erasure::Immutable<Callable<int(Hint<1>,int) const, int(int) const>,Fooable<void() const>>;
using MutableObject = rtci_erasure::Mutable<sizeof(void*)*2, Callable<int(int) const, int(Hint<1>,int) const, int(Hint<2>)>, Fooable<void()>>;

struct Test {
  int operator() ( int ) const { std::cout << "call" << std::endl; return 0; }
  int operator() ( Hint<1>, int ) const { std::cout << "call Hint<1>" << std::endl; return 0; }
  int operator() ( Hint<2> ) { std::cout << "call Hint<2>" << std::endl; return 0; }
  void foo() const { std::cout << "foo" << std::endl; }
};

int main()
{
  {
    ImmutableObject object{ Test{} };
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
    MutableObject object{ Test{} };
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
