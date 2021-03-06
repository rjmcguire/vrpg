#include "worldtypes.h"

/// v is zero based destination coordinates
void VolumeData::putLayer(Vector3d v, cell_t * layer, int dx, int dz, int stripe) {
	cell_t * dst = _data + ((v.y << (ROW_BITS * 2)) | (v.z << ROW_BITS) | v.x);
	//int nzcount = 0;
	for (int z = 0; z < dz; z++) {
		memcpy(dst, layer, sizeof(cell_t) * dx);
		//for (int x = 0; x < dx; x++)
		//	if (dst[x])
		//		nzcount++;
		layer += stripe;
		dst += ROW_SIZE;

	}
	//CRLog::trace("  non-zero cells copied: %d", nzcount);
}

// DirEx to DirMask
const DirMask DIR_TO_MASK[] = {
	MASK_NORTH,
	MASK_SOUTH,
	MASK_WEST,
	MASK_EAST,
	MASK_UP,
	MASK_DOWN,
	MASK_WEST_UP,
	MASK_EAST_UP,
	MASK_WEST_DOWN,
	MASK_EAST_DOWN,
	MASK_NORTH_WEST,
	MASK_NORTH_EAST,
	MASK_NORTH_UP,
	MASK_NORTH_DOWN,
	MASK_NORTH_WEST_UP,
	MASK_NORTH_EAST_UP,
	MASK_NORTH_WEST_DOWN,
	MASK_NORTH_EAST_DOWN,
	MASK_SOUTH_WEST,
	MASK_SOUTH_EAST,
	MASK_SOUTH_UP,
	MASK_SOUTH_DOWN,
	MASK_SOUTH_WEST_UP,
	MASK_SOUTH_EAST_UP,
	MASK_SOUTH_WEST_DOWN,
	MASK_SOUTH_EAST_DOWN
};

const Vector3d DIRECTION_VECTORS[6] = {
	Vector3d(0, 0, -1),
	Vector3d(0, 0, 1),
	Vector3d(-1, 0, 0),
	Vector3d(1, 0, 0),
	Vector3d(0, 1, 0),
	Vector3d(0, -1, 0)
};

static DirEx NEAR_DIRECTIONS_FOR[6 * 8] = {
	// NORTH
	DIR_EAST, DIR_UP, DIR_WEST, DIR_DOWN,		DIR_EAST_UP, DIR_WEST_UP, DIR_WEST_DOWN, DIR_EAST_DOWN,
	// SOUTH
	DIR_EAST, DIR_UP, DIR_WEST, DIR_DOWN,		DIR_EAST_UP, DIR_WEST_UP, DIR_WEST_DOWN, DIR_EAST_DOWN,
	// WEST
	DIR_SOUTH, DIR_UP, DIR_NORTH, DIR_DOWN,		DIR_SOUTH_UP, DIR_NORTH_UP, DIR_NORTH_DOWN, DIR_SOUTH_DOWN,
	// EAST
	DIR_SOUTH, DIR_UP, DIR_NORTH, DIR_DOWN,		DIR_SOUTH_UP, DIR_NORTH_UP, DIR_NORTH_DOWN, DIR_SOUTH_DOWN,
	// UP
	DIR_EAST, DIR_NORTH, DIR_WEST, DIR_SOUTH,	DIR_NORTH_EAST, DIR_NORTH_WEST, DIR_SOUTH_WEST, DIR_SOUTH_EAST,
	// DOWN
	DIR_EAST, DIR_NORTH, DIR_WEST, DIR_SOUTH,	DIR_NORTH_EAST, DIR_NORTH_WEST, DIR_SOUTH_WEST, DIR_SOUTH_EAST,
};

static const char * dir_names[] = {
	"NORTH",
	"SOUTH",
	"WEST",
	"EAST",
	"UP",
	"DOWN",
};

VolumeData::VolumeData(int distBits) : MAX_DIST_BITS(distBits) {
	ROW_BITS = MAX_DIST_BITS + 1;
	MAX_DIST = 1 << MAX_DIST_BITS;
	ROW_SIZE = 1 << (MAX_DIST_BITS + 1);
	DATA_SIZE = ROW_SIZE * ROW_SIZE * ROW_SIZE;
	ROW_MASK = ROW_SIZE - 1;
	_data = new cell_t[DATA_SIZE];
	clear();
	for (int i = 0; i < 64; i++) {
		int delta = 0;
		if (i & MASK_WEST)
			delta--;
		if (i & MASK_EAST)
			delta++;
		if (i & MASK_NORTH)
			delta -= ROW_SIZE;
		if (i & MASK_SOUTH)
			delta += ROW_SIZE;
		if (i & MASK_UP)
			delta += ROW_SIZE * ROW_SIZE;
		if (i & MASK_DOWN)
			delta -= ROW_SIZE * ROW_SIZE;
		directionDelta[i] = delta;
	}
	for (int d = DIR_MIN; d < DIR_MAX; d++) {
		directionExDelta[d] = directionDelta[DIR_TO_MASK[d]];
	}
	for (int d = 0; d < 6; d++) {
		DirEx * dirs = NEAR_DIRECTIONS_FOR + 8 * d;
		mainDirectionDeltas[d][0] = directionExDelta[d];
		mainDirectionDeltasNoForward[d][0] = 0;
		for (int i = 0; i < 8; i++) {
			mainDirectionDeltas[d][1 + i] = mainDirectionDeltas[d][0] + directionExDelta[dirs[i]];
			mainDirectionDeltasNoForward[d][1 + i] = directionExDelta[dirs[i]];
		}
		int delta = directionExDelta[d];
		Vector3d pt = indexToPoint(getIndex(Vector3d()) + delta);
		//CRLog::trace("Direction : %d %s (%d,%d,%d)", d, dir_names[d], pt.x, pt.y, pt.z);
		for (int i = 0; i < 9; i++) {
			int delta = mainDirectionDeltas[d][i];
			Vector3d pt = indexToPoint(getIndex(Vector3d()) + delta);
			//CRLog::trace("   [%d] Delta         : %2d,%2d,%2d", i, pt.x, pt.y, pt.z);
		}
		for (int i = 0; i < 9; i++) {
			int delta = mainDirectionDeltasNoForward[d][i];
			Vector3d pt = indexToPoint(getIndex(Vector3d()) + delta);
			//CRLog::trace("   [%d] Delta_no_fwd  : %2d,%2d,%2d", i, pt.x, pt.y, pt.z);
		}
	}
}

void VolumeData::fillLayer(int y, cell_t cell) {
	y += MAX_DIST;
	if (y >= 0 && y < ROW_SIZE) {
		int index = y << (ROW_BITS * 2);
		memset(_data + index, cell, ROW_SIZE * ROW_SIZE);
	}
}

void VolumeData::getNearCellsForDirection(int index, DirEx direction, CellToVisit cells[9]) {
	int * deltas = mainDirectionDeltas[direction];
	CellToVisit * cell = cells + 0;
	cell->index = index + *deltas;
	cell->cell = _data[cell->index];
	cell->dir = direction;
	//if (!cell->cell || cell->cell == VISITED_CELL) {
	for (int i = 0; i < 8; i++) {
		cell++;
		deltas++;
		cell->index = index + *deltas;
		cell->cell = _data[cell->index];
		cell->dir = direction;
	}
	//}
}

void VolumeData::getNearCellsForDirectionNoForward(int index, DirEx direction, CellToVisit cells[9]) {
	int * deltas = mainDirectionDeltasNoForward[direction];
	CellToVisit * cell = cells + 0;
	cell->index = index + *deltas;
	cell->cell = _data[cell->index];
	cell->dir = direction;
	//if (!cell->cell || cell->cell == VISITED_CELL) {
	for (int i = 0; i < 8; i++) {
		cell++;
		deltas++;
		cell->index = index + *deltas;
		cell->cell = _data[cell->index];
		cell->dir = direction;
	}
	//}
}

void VolumeData::getNearCellsForDirection(int index, DirEx direction, cell_t cells[9]) {
	int * deltas = mainDirectionDeltas[direction];
	for (int i = 0; i < 9; i++)
		cells[i] = _data[index + deltas[i]];
}
void VolumeData::getNearCellsForDirectionNoForward(int index, DirEx direction, cell_t cells[9]) {
	int * deltas = mainDirectionDeltasNoForward[direction];
	for (int i = 0; i < 9; i++)
		cells[i] = _data[index + deltas[i]];
}


void DirectionHelper::start(int index, DirEx direction) {
	dir = direction;
	oldcells.clear();
	newcells.clear();
	newcells.append(index);
}
void DirectionHelper::nextDistance() {
	forwardCellCount = 0;
	newcells.swap(oldcells);
	newcells.clear();
	for (int i = 0; i < 4; i++) {
		spreadcells.clear();
	}
}
void DirectionHelper::prepareSpreading() {
	forwardCellCount = newcells.length();
}


static lUInt64 seedUniquifier = 8682522807148012L;

Random::Random() {
	setSeed(++seedUniquifier + GetCurrentTimeMillis() * RANDOM_MULTIPLIER);
}

int Random::nextInt(int n) {
	if ((n & -n) == n)  // i.e., n is a power of 2
		return (int)((n * (lUInt64)next(31)) >> 31);
	int bits, val;
	do {
		bits = next(31);
		val = bits % n;
	} while (bits - val + (n - 1) < 0);
	return val;
}
