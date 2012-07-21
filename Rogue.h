
#pragma once

#include "Vector.h"
#include "Grid.h"

typedef Vector<int,2> Vec;

struct Tile
{
    bool seen      : 1;
    bool visible   : 1;
    bool highlight : 1; 
    char c;

    Tile() : c(' ') { init(); }

    // Allow implicit construction.
    Tile( char c ) : c(c) { init(); }

  private:
    void init() { seen = visible = highlight = false; }
};

// main.cpp
extern Grid<Tile> grid;
