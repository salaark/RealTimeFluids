#pragma once

#include <glm/glm.hpp>

static enum CellType {
	FLUID,
	AIR
};

static struct GridCell {
	CellType cellType;
	glm::vec3 worldPosition;
	float pressure;
	float tempPressure;
	glm::vec3 velocity;
	glm::vec3 tempVelocity;
};

static struct MarkerParticle {
	glm::vec3 worldPosition;
	glm::vec3 color;
};

static float TIME_STEP = 1.0 / 30.0;

static int GRID_X = 16;
static int GRID_Y = 32;
static int GRID_Z = 16;

static float DENSITY = 1.0;

static int NUM_CELLS = GRID_X * GRID_Y * GRID_Z;

static float CELL_WIDTH = 1.0;
static int NUM_MARKER_PARTICLES = 10000;

static int blockSize = 256;

static GridCell *dev_gridCells;
static MarkerParticle *dev_markerParticles;

static int BLOCKS_PARTICLES = (NUM_MARKER_PARTICLES + blockSize - 1) / blockSize;
static int BLOCKS_CELLS = (NUM_CELLS + blockSize - 1) / blockSize;

void initSim();
void iterateSim();
void fillVBOsWithMarkerParticles(void *vbo);