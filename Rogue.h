
#pragma once

#include "Vector.h"
#include "Grid.h"

typedef Vector<int,2> Vec;

struct Tile
{
    bool seen, visible;
    char c;

    Tile() : seen(false), visible(false), c(' ') {}

    // Allow implicit construction.
    Tile( char c ) : seen(false), visible(false), c(c) {}
};

// main.cpp
extern Grid<Tile> grid;
