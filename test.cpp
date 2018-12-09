#include "Erasure.hpp"
#include <iostream>

template < int index >
struct Hint{};

ERASURE_MIXIN_METHOD( Callable, operator() );
ERASURE_MIXIN_METHOD( Fooable, foo );

using ImmutableObject = erasure::Immutable<Callable<int(int) const, int(Hint<1>,int) const>>;
using MutableObject = erasure::Mutable<sizeof(void*)*2, Callable<int(int) const, int(Hint<1>,int) const, int(Hint<2>)>, Fooable<void()>>;


struct Test {
  int operator() ( int ) const { std::cout << "call" << std::endl; return 0; }
  int operator() ( Hint<1>, int ) const { std::cout << "call Hint<1>" << std::endl; return 0; }
  int operator() ( Hint<2> ) { std::cout << "call Hint<2>" << std::endl; return 0; }
  void foo() { std::cout << "foo" << std::endl; }
};

int main()
{
  {
    ImmutableObject object{ Test{} };
    object( 10 );
    ImmutableObject other = std::move(object);
    other( 5 );
    other( Hint<1>{}, 2 );
  }
  {
    MutableObject object{ Test{} };
    object( Hint<2>{} );
  }
  return 0;
}
