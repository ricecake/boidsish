#version 460 core

layout(location = 0) out float out_Depth;

layout(binding = 0) uniform sampler2D u_srcDepth;

void main() {
	// Current fragment coordinate in pixels (destination space)
	ivec2 pixel = ivec2(gl_FragCoord.xy);
	ivec2 srcCoord = pixel * 2;

	vec2 srcSize = vec2(textureSize(u_srcDepth, 0));
	vec2 uv = (vec2(srcCoord) + vec2(1.0)) / srcSize;
	vec4 depths = textureGather(u_srcDepth, uv);

	out_Depth = max(max(depths.x, depths.y), max(depths.z, depths.w));
}
