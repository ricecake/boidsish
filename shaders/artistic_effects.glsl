#ifndef ARTISTIC_EFFECTS_GLSL
#define ARTISTIC_EFFECTS_GLSL

layout(std140) uniform ArtisticEffects {
	bool blackAndWhite;
	bool negative;
	bool shimmery;
	bool glitched;
	bool wireframe;
};
