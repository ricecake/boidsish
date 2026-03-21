struct TrailPoint {
	vec4 pos;   // xyz: position
	vec4 color; // rgb: color
};

struct TrailInstance {
	uint  points_offset; // Offset in TrailPoints SSBO
	uint  head;          // Ring buffer head index
	uint  tail;          // Ring buffer tail index
	uint  max_points;    // Max points in the ring buffer
	float thickness;
	uint  vertex_offset; // Offset in the generated VBO (in vertices)
	uint  is_full;
	uint  flags; // 1: iridescent, 2: rocket, 4: pbr
	float roughness;
	float metallic;
	uint  _padding[2];
};

struct TrailVertex {
	vec4 pos;    // xyz: position, w: progress
	vec4 normal; // xyz: normal, w: ?
	vec4 color;  // rgb: color, w: ?
};

struct TrailSpinePoint {
	vec4 pos;     // xyz: position
	vec4 color;   // rgb: color
	vec4 tangent; // xyz: tangent
	vec4 normal;  // xyz: normal
};
