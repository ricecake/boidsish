
uint get_cell_idx(vec3 pos, float cellSize, uint gridSize) {
	ivec3 cellPos = ivec3(floor(pos / cellSize));
	// Spatial hash using large primes
	uint h = uint(cellPos.x * 73856093) ^ uint(cellPos.y * 19349663) ^ uint(cellPos.z * 83492791);
	return h % gridSize;
}

