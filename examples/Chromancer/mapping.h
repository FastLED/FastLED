/*
 * Maps hex topology onto LED's
 * (C) Voidstar Lab LLC 2021
 */

#ifndef MAPPING_H_
#define MAPPING_H_

// I accidentally noted these down 1-indexed and I'm too tired to adjust them
#define headof(S) ((S - 1) * 14)
#define tailof(S) (headof(S) + 13)

// Beam 0 is at 12:00 and advance clockwise
// -1 means nothing connected on that side
int nodeConnections[25][6] = {
  {-1, -1, 1, -1, 0, -1},
  {-1, -1, 3, -1, 2, -1},
  {-1, -1, 5, -1, 4, -1},
  {-1, 0, 6, 12, -1, -1},
  {-1, 2, 8, 14, 7, 1},
  
  {-1, 4, 10, 16, 9, 3},
  {-1, -1, -1, 18, 11, 5},
  {-1, 7, -1, 13, -1, 6},
  {-1, 9, -1, 15, -1, 8},
  {-1, 11, -1, 17, -1, 10},
  
  {12, -1, 19, -1, -1, -1},
  {14, -1, 21, -1, 20, -1},
  {16, -1, 23, -1, 22, -1},
  {18, -1, -1, -1, 24, -1},
  {13, 20, 25, 29, -1, -1},
  
  {15, 22, 27, 31, 26, 21},
  {17, 24, -1, 33, 28, 23},
  {-1, 26, -1, 30, -1, 25},
  {-1, 28, -1, 32, -1, 27},
  {29, -1, 34, -1, -1, -1},
  
  {31, -1, 36, -1, 35, -1},
  {33, -1, -1, -1, 37, -1},
  {30, 35, 38, -1, -1, 34},
  {32, 37, -1, -1, 39, 36},
  {-1, 39, -1, -1, -1, 38}
};

// First member: Node closer to ceiling
// Second: Node closer to floor
int segmentConnections[40][2] = {
  {0, 3},
  {0, 4},
  {1, 4},
  {1, 5},
  {2, 5},
  {2, 6},
  {3, 7},
  {4, 7},
  {4, 8},
  {5, 8},
  {5, 9},
  {6, 9}, // ayy
  {3, 10},
  {7, 14},
  {4, 11},
  {8, 15},
  {5, 12},
  {9, 16},
  {6, 13},
  {10, 14},
  {11, 14},
  {11, 15},
  {12, 15},
  {12, 16},
  {13, 16},
  {14, 17},
  {15, 17},
  {15, 18},
  {16, 18},
  {14, 19},
  {17, 22},
  {15, 20},
  {18, 23},
  {16, 21},
  {19, 22},
  {20, 22},
  {20, 23},
  {21, 23},
  {22, 24},
  {23, 24}
};

// First member: Strip number
// Second: LED index closer to ceiling
// Third: LED index closer to floor
int ledAssignments[40][3] = {
  {2, headof(3), tailof(3)},
  {2, tailof(2), headof(2)},
  {1, headof(10), tailof(10)},
  {1, tailof(9), headof(9)},
  {1, headof(4), tailof(4)},
  {1, tailof(3), headof(3)},
  
  {2, tailof(6), headof(6)},
  {3, tailof(11), headof(11)},
  {1, headof(11), tailof(11)},
  {1, tailof(8), headof(8)},
  {1, headof(12), tailof(12)},
  {0, tailof(11), headof(11)},
  
  {2, headof(4), tailof(4)},
  {3, tailof(10), headof(10)},
  {2, tailof(1), headof(1)},
  {1, tailof(7), headof(7)},
  {1, headof(5), tailof(5)},
  {0, tailof(10), headof(10)},
  {1, tailof(2), headof(2)},
  
  {2, headof(5), tailof(5)},
  {3, tailof(4), headof(4)},
  {3, headof(5), tailof(5)},
  {0, headof(5), tailof(5)},
  {0, tailof(4), headof(4)},
  {1, tailof(1), headof(1)},
  
  {3, tailof(9), headof(9)},
  {0, headof(6), tailof(6)},
  {1, tailof(6), headof(6)},
  {0, tailof(9), headof(9)},
  
  {3, tailof(3), headof(3)},
  {3, tailof(8), headof(8)},
  {3, headof(6), tailof(6)},
  {0, tailof(8), headof(8)},
  {0, tailof(3), headof(3)},

  {3, tailof(2), headof(2)},
  {3, headof(7), tailof(7)},
  {0, headof(7), tailof(7)},
  {0, tailof(2), headof(2)},
  
  {3, tailof(1), headof(1)},
  {0, tailof(1), headof(1)}
};

// Border nodes are on the very edge of the network.
// Ripples fired here don't look very impressive.
int numberOfBorderNodes = 10;
int borderNodes[] = {0, 1, 2, 3, 6, 10, 13, 19, 21, 24};

// Cube nodes link three equiangular segments
// Firing ripples that always turn in one direction will draw a cube
int numberOfCubeNodes = 8;
int cubeNodes[] = {7, 8, 9, 11, 12, 17, 18};

// Firing ripples that always turn in one direction will draw a starburst
int starburstNode = 15;

#endif
