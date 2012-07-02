
#pragma once

#include <algorithm>
#include <functional>
#include <iterator>
#include <array>


#include <iostream>
template< typename Container >
void print( const char* const msg, const Container& v )
{
    std::cout << msg << "\n";
    std::copy( v.begin(), v.end(), std::ostream_iterator<float>(std::cout, " ") );
    std::cout << std::endl;
}

namespace pure {

template< typename C1, typename C2 >
constexpr C1 concat( C1 a, const C2& b )
{
    a.reserve( a.size() + b.size() );
    std::copy( std::begin(b), std::end(b), std::back_inserter(a) );
    return a;
}

template< typename Cx1, typename Cx2, typename ... Cxs >
constexpr Cx1 concat( Cx1&& a, const Cx2& b, const Cxs& ... c )
{
    return concat( concat(std::forward<Cx1>(a), b), c... );
}

template< typename Container >
constexpr bool ordered( const Container& c )
{
    return c.size() <= 1
        or std::mismatch ( 
            std::begin(c), std::end(c)-1, 
            std::begin(c)+1, 
            std::less_equal<int>() 
        ).second == std::end(c);
}

//template< typename Container, typename F >
//constexpr Container map( const F& f, const Container& cont ) 
//{
//    Container cont2; cont2.reserve( cont.size() );
//    std::transform( std::begin(cont), std::end(cont), std::back_inserter(cont2), f );
//    return cont2;
//}

// TODO: The is optimal for rvalues, but the above should be slightly more
// efficient for lvalues. Enabling both causes ambiguity and making this one
// take an rval-ref would cause the other to never get called.
template< typename Container, typename F >
constexpr Container map( const F& f, Container cont ) 
{
    std::transform( std::begin(cont), std::end(cont), std::begin(cont), f );
    return cont;
}

// TODO: Similar problem as with map. The optimal solution for lvalues should
// do a copy-if.
template< typename Container, typename F >
constexpr Container filter( const F& f, Container cont )
{
    typedef typename std::remove_reference<Container>::type::value_type T;
    cont.erase ( 
        std::remove_if ( 
            std::begin(cont), std::end(cont), 
            [&](const T& t){ return not f(t); } 
        ),
        std::end( cont )
    );
    return cont;
}

template< typename Container, typename F >
constexpr bool all( const F& f, const Container& cont )
{
    return std::all_of( begin(cont), end(cont), f );
}

template< typename Container, typename Value, typename F >
constexpr Value fold( const F& f, const Value& val, const Container& cont )
{
    return std::accumulate( begin(cont), end(cont), val, f );
}

template< typename Container, typename F >
void for_each( const F& f, const Container& cont )
{
    std::for_each( std::begin(cont), std::end(cont), f );
}

template< typename X, typename F, typename ... Fs >
constexpr std::array<X,sizeof...(Fs)+1> cleave( X x, const F& f, const Fs& ... fs ) 
{
    return std::array<X,sizeof...(Fs)+1>{ f(x), fs(x)... };
}

template< class T, unsigned int N, class F >
constexpr std::array<T,N> generate( F f )
{
    std::array<T,N> cont;
    std::generate( std::begin(cont), std::end(cont), f );
    return cont;
}

#include <vector>
template< class T, class F >
constexpr std::vector<T> generate( F f, unsigned int n )
{
    std::vector<T> c; c.reserve(n);
    while( n-- )
        c.push_back( f() );
    return c;
}

} // namespace pure

