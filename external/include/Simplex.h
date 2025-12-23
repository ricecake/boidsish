#pragma once

#include <functional>
#include <array>
#include <random>
#include <math.h>
#include <stdlib.h>
#include <glm/geometric.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat2x2.hpp>

class SimplexNoise {
public:
	#ifdef SIMPLEX_INTEGER_LUTS
	    typedef uint8_t LutType;
	#else
	    typedef unsigned char LutType;
	#endif

    SimplexNoise( uint32_t seed = 0 );

    float noise( float x );
    float noise( const glm::vec2 &v );
    float noise( const glm::vec3 &v );
    float noise( const glm::vec4 &v );

    float ridgedNoise( float x );
    float ridgedNoise( const glm::vec2 &v );
    float ridgedNoise( const glm::vec3 &v );
    float ridgedNoise( const glm::vec4 &v );

    glm::vec2 dnoise( float x );
    glm::vec3 dnoise( const glm::vec2 &v );
    glm::vec4 dnoise( const glm::vec3 &v );
    typedef std::array<float,5> vec5;
    vec5	dnoise( const glm::vec4 &v );

    float worleyNoise( const glm::vec2 &v );
    float worleyNoise( const glm::vec3 &v );
    float worleyNoise( const glm::vec2 &v, float falloff );
    float worleyNoise( const glm::vec3 &v, float falloff );

    float flowNoise( const glm::vec2 &v, float angle );
    float flowNoise( const glm::vec3 &v, float angle );

    glm::vec3 dFlowNoise( const glm::vec2 &v, float angle );
    glm::vec4 dFlowNoise( const glm::vec3 &v, float angle );

    glm::vec2 curlNoise( const glm::vec2 &v );
    glm::vec2 curlNoise( const glm::vec2 &v, float t );
    glm::vec2 curlNoise( const glm::vec2 &v, uint8_t octaves, float lacunarity, float gain );
    glm::vec3 curlNoise( const glm::vec3 &v );
    glm::vec3 curlNoise( const glm::vec3 &v, float t );
    glm::vec3 curlNoise( const glm::vec3 &v, uint8_t octaves, float lacunarity, float gain );

    glm::vec2 curl( const glm::vec2 &v, const std::function<float(const glm::vec2&)> &potential, float delta = 1e-4f );
    glm::vec3 curl( const glm::vec3 &v, const std::function<glm::vec3(const glm::vec3&)> &potential, float delta = 1e-4f );

    float fBm( float x, uint8_t octaves = 4, float lacunarity = 2.0f, float gain = 0.5f );
    float fBm( const glm::vec2 &v, uint8_t octaves = 4, float lacunarity = 2.0f, float gain = 0.5f );
    float fBm( const glm::vec3 &v, uint8_t octaves = 4, float lacunarity = 2.0f, float gain = 0.5f );
    float fBm( const glm::vec4 &v, uint8_t octaves = 4, float lacunarity = 2.0f, float gain = 0.5f );

    float worleyfBm( const glm::vec2 &v, uint8_t octaves = 4, float lacunarity = 2.0f, float gain = 0.5f );
    float worleyfBm( const glm::vec3 &v, uint8_t octaves = 4, float lacunarity = 2.0f, float gain = 0.5f );
    float worleyfBm( const glm::vec2 &v, float falloff, uint8_t octaves = 4, float lacunarity = 2.0f, float gain = 0.5f );
    float worleyfBm( const glm::vec3 &v, float falloff, uint8_t octaves = 4, float lacunarity = 2.0f, float gain = 0.5f );

    glm::vec2 dfBm( float x, uint8_t octaves = 4, float lacunarity = 2.0f, float gain = 0.5f );
    glm::vec3 dfBm( const glm::vec2 &v, uint8_t octaves = 4, float lacunarity = 2.0f, float gain = 0.5f );
    glm::vec4 dfBm( const glm::vec3 &v, uint8_t octaves = 4, float lacunarity = 2.0f, float gain = 0.5f );
    vec5	dfBm( const glm::vec4 &v, uint8_t octaves = 4, float lacunarity = 2.0f, float gain = 0.5f );

    float ridgedMF( float x, float ridgeOffset = 1.0f, uint8_t octaves = 4, float lacunarity = 2.0f, float gain = 0.5f );
    float ridgedMF( const glm::vec2 &v, float ridgeOffset = 1.0f, uint8_t octaves = 4, float lacunarity = 2.0f, float gain = 0.5f );
    float ridgedMF( const glm::vec3 &v, float ridgeOffset = 1.0f, uint8_t octaves = 4, float lacunarity = 2.0f, float gain = 0.5f );
    float ridgedMF( const glm::vec4 &v, float ridgeOffset = 1.0f, uint8_t octaves = 4, float lacunarity = 2.0f, float gain = 0.5f );

    float iqfBm( const glm::vec2 &v, uint8_t octaves = 4, float lacunarity = 2.0f, float gain = 0.5f );
    float iqfBm( const glm::vec3 &v, uint8_t octaves = 4, float lacunarity = 2.0f, float gain = 0.5f );

    float iqMatfBm( const glm::vec2 &v, uint8_t octaves = 4, const glm::mat2 &mat = glm::mat2( 1.6, -1.2, 1.2, 1.6 ), float gain = 0.5f );

protected:
    LutType perm[512];

    float grad( int hash, float x );
    float grad( int hash, float x, float y );
    float grad( int hash, float x, float y , float z );
    float grad( int hash, float x, float y, float z, float t );

    void grad1( int hash, float *gx );
    void grad2( int hash, float *gx, float *gy );
    void grad3( int hash, float *gx, float *gy, float *gz );
    void grad4( int hash, float *gx, float *gy, float *gz, float *gw);

    void gradrot2( int hash, float sin_t, float cos_t, float *gx, float *gy );
    void gradrot3( int hash, float sin_t, float cos_t, float *gx, float *gy, float *gz );
    float graddotp2( float gx, float gy, float x, float y );
    float graddotp3( float gx, float gy, float gz, float x, float y, float z );
};
