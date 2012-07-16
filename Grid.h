
#include "Vector.h"

#include <iterator>
#include <algorithm>

#pragma once

typedef Vector<int,2> Vec;

template< typename T>
struct OffsetIterator : public std::iterator<std::random_access_iterator_tag, T>
{
    typedef OffsetIterator iterator;
    typedef T& reference;
    typedef const T& const_reference;
    
    T* cur;
    size_t offset;

    OffsetIterator( T* start, size_t offset=1 )
        : cur(start), offset(offset) {}

    iterator& operator++() { cur += offset; return *this; }    
    iterator& operator--() { cur -= offset; return *this; }
    iterator  operator++(int) { iterator tmp=*this; ++*this; return tmp; }
    iterator  operator--(int) { iterator tmp=*this; --*this; return tmp; }

    iterator operator+(int x) { return iterator(cur+offset*x); }
    iterator operator-(int x) { return iterator(cur-offset*x); }

    int operator-( iterator other ) { return (cur - other.cur) / offset; }

    reference operator*() { return *cur; }
    const_reference operator*() const { return *cur; }
};

template< typename T >
struct RoomIterator  : public std::iterator< std::bidirectional_iterator_tag, T>
{
    typedef RoomIterator iterator;
    typedef char& reference;
    typedef const char& const_reference;

    T* base;
    T* cur;
    size_t offset, width;
    
    RoomIterator( T* start, size_t offset, size_t width ) 
        : base(start), cur(base), offset(offset), width(width) {}

    iterator& operator++() 
    { 
        cur += 1;
        size_t diff = cur - base;
        if( diff >= width ) {
            base += offset;
            cur = base + (diff - width);
        }

        return *this; 
    }    
    iterator  operator++(int) { iterator tmp=*this; ++*this; return tmp; }

    iterator& operator--()
    {
        if( cur <= base ) {
            base -= offset;
            cur = base + width - 1;
        } else {
            cur--;
        }

        return *this;
    }
    iterator operator--(int) { iterator tmp=*this; ++*this; return tmp; }

    reference operator*() { return *cur; }
};

template< typename T >
bool operator == ( const RoomIterator<T>& a, const RoomIterator<T>& b )
{ return a.base == b.base or a.cur == b.cur; } 
template< typename T >
bool operator == ( const OffsetIterator<T>& a, const OffsetIterator<T>& b )
{ return a.cur == b.cur and a.offset == b.offset; }

template< typename T >
bool operator != ( const OffsetIterator<T>& a, const OffsetIterator<T>& b )
{ return not ( a == b ); }
template< typename T >
bool operator != ( const RoomIterator<T>& a, const RoomIterator<T>& b )
{ return not ( a == b ); }

struct Room
{
    static const int MINLEN;

    size_t left, right, up, down;
    Room(size_t l, size_t r, size_t u, size_t d);
    Room( const Room& other );
    Room() {}
};

// Split the room horizontally or vertically.
// len specifies the minimum length of the split room.
std::pair<Room,Room> hsplit( const Room& r, int len );
std::pair<Room,Room> vsplit( const Room& r, int len );

template< typename Tile >
struct Grid
{
    typedef Tile& reference;
    typedef OffsetIterator<Tile> iterator;
    typedef RoomIterator<Tile> room_iterator;
    typedef const Tile& const_reference;
    typedef OffsetIterator<const Tile> const_iterator;
    typedef RoomIterator<const Tile> const_room_iterator;

    Tile* tiles;
    size_t width, height;

    Grid() : tiles(0), width(0), height(0) {}
    Grid( size_t w, size_t h, const Tile& t )
        : tiles(new Tile[ w * h ]), width(w), height(h)
    { std::fill_n( tiles, area(), t ); }

    ~Grid() { if(tiles) delete [] tiles; }

    void reset( size_t w, size_t h, const Tile& t )
    {
        width = w;
        height = h;
        if( tiles )
            delete [] tiles;
        tiles = new Tile[ area() ];
        std::fill_n( tiles, area(), t );
    }

    size_t area() const { return width * height; }

    reference get( size_t x, size_t y ) 
    { return tiles[ y*width + x ]; }
    const_reference get( size_t x, size_t y ) const 
    { return tiles[ y*width + x ]; }

    template< typename U > reference get( const Vector<U,2>& pos ) 
    { return get( pos.x(), pos.y() ); }
    template< typename U > const_reference get( const Vector<U,2>& pos ) const
    { return get( pos.x(), pos.y() ); }

    iterator row_begin( size_t n ) { return iterator(tiles + n*width); }
    iterator row_end( size_t n )   { return iterator(tiles + (n+1) * width); }
    const_iterator row_begin( size_t n ) const 
    {return const_iterator(tiles + n*width); }
    const_iterator row_end( size_t n )   const 
    { return const_iterator(tiles + (n+1) * width); }

    iterator col_begin( size_t n ) 
    { return iterator(tiles + n, width); }
    iterator col_end( size_t n ) 
    { return iterator(tiles + area() + n, width); }
    const_iterator col_begin( size_t n ) const 
    { return const_iterator(tiles + n, width); }
    const_iterator col_end( size_t n ) const 
    { return const_iterator(tiles + area() + n, width); }

    room_iterator reg_begin( const Room& r )
    {
        return room_iterator( &get(r.left, r.up), 
                                width, r.right-r.left+1 );
    }
    room_iterator reg_end( const Room& r )
    {
        return room_iterator( &get(r.left, r.down+1), 
                                width, r.right-r.left+1 );
    }
    const_room_iterator reg_begin( const Room& r ) const
    {
        return const_room_iterator( &get(r.left, r.up), 
                                      width, r.right-r.left+1 );
    }
    const_room_iterator reg_end( const Room& r ) const
    {
        return const_room_iterator( &get(r.left, r.down+1), 
                                      width, r.right-r.left+1 );
    }

    iterator begin() { return tiles; }
    iterator end()   { return tiles + area() + 1; }
    const_iterator end()   const { return tiles + area() + 1; }
    const_iterator begin() const { return tiles; }
};
