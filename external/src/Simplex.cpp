#include "Simplex.h"
#include <random>

SimplexNoise::SimplexNoise( uint32_t seed )
{
    std::random_device rd;
    std::mt19937 gen( rd() );
    gen.seed( seed );
    std::uniform_int_distribution<> distribution( 1, 255 );
    for( size_t i = 0; i < 256; ++i ) {
        perm[i] = perm[i + 256] = distribution( gen );
    }
}

#define FASTFLOOR(x) ( ((x)>0) ? ((int)x) : (((int)x)-1) )

namespace {
	static float grad2lut[8][2] = {
		{ -1.0f, -1.0f }, { 1.0f, 0.0f } , { -1.0f, 0.0f } , { 1.0f, 1.0f } ,
		{ -1.0f, 1.0f } , { 0.0f, -1.0f } , { 0.0f, 1.0f } , { 1.0f, -1.0f }
	};
	static float grad3lut[16][3] = {
		{ 1.0f, 0.0f, 1.0f }, { 0.0f, 1.0f, 1.0f },
		{ -1.0f, 0.0f, 1.0f }, { 0.0f, -1.0f, 1.0f },
		{ 1.0f, 0.0f, -1.0f }, { 0.0f, 1.0f, -1.0f },
		{ -1.0f, 0.0f, -1.0f }, { 0.0f, -1.0f, -1.0f },
		{ 1.0f, -1.0f, 0.0f }, { 1.0f, 1.0f, 0.0f },
		{ -1.0f, 1.0f, 0.0f }, { -1.0f, -1.0f, 0.0f },
		{ 1.0f, 0.0f, 1.0f }, { -1.0f, 0.0f, 1.0f },
		{ 0.0f, 1.0f, -1.0f }, { 0.0f, -1.0f, -1.0f }
	};
	static float grad4lut[32][4] = {
		{ 0.0f, 1.0f, 1.0f, 1.0f }, { 0.0f, 1.0f, 1.0f, -1.0f }, { 0.0f, 1.0f, -1.0f, 1.0f }, { 0.0f, 1.0f, -1.0f, -1.0f },
		{ 0.0f, -1.0f, 1.0f, 1.0f }, { 0.0f, -1.0f, 1.0f, -1.0f }, { 0.0f, -1.0f, -1.0f, 1.0f }, { 0.0f, -1.0f, -1.0f, -1.0f },
		{ 1.0f, 0.0f, 1.0f, 1.0f }, { 1.0f, 0.0f, 1.0f, -1.0f }, { 1.0f, 0.0f, -1.0f, 1.0f }, { 1.0f, 0.0f, -1.0f, -1.0f },
		{ -1.0f, 0.0f, 1.0f, 1.0f }, { -1.0f, 0.0f, 1.0f, -1.0f }, { -1.0f, 0.0f, -1.0f, 1.0f }, { -1.0f, 0.0f, -1.0f, -1.0f },
		{ 1.0f, 1.0f, 0.0f, 1.0f }, { 1.0f, 1.0f, 0.0f, -1.0f }, { 1.0f, -1.0f, 0.0f, 1.0f }, { 1.0f, -1.0f, 0.0f, -1.0f },
		{ -1.0f, 1.0f, 0.0f, 1.0f }, { -1.0f, 1.0f, 0.0f, -1.0f }, { -1.0f, -1.0f, 0.0f, 1.0f }, { -1.0f, -1.0f, 0.0f, -1.0f },
		{ 1.0f, 1.0f, 1.0f, 0.0f }, { 1.0f, 1.0f, -1.0f, 0.0f }, { 1.0f, -1.0f, 1.0f, 0.0f }, { 1.0f, -1.0f, -1.0f, 0.0f },
		{ -1.0f, 1.0f, 1.0f, 0.0f }, { -1.0f, 1.0f, -1.0f, 0.0f }, { -1.0f, -1.0f, 1.0f, 0.0f }, { -1.0f, -1.0f, -1.0f, 0.0f }
	};
}

#define F2 0.366025403f
#define G2 0.211324865f
#define F3 0.333333333f
#define G3 0.166666667f
#define F4 0.309016994f
#define G4 0.138196601f

float SimplexNoise::grad( int hash, float x ) {
	int h = hash & 15;
	float grad = 1.0f + (h & 7);
	if (h&8) grad = -grad;
	return ( grad * x );
}
float SimplexNoise::grad( int hash, float x, float y ) {
	int h = hash & 7;
	return grad2lut[h][0]*x + grad2lut[h][1]*y;
}
float SimplexNoise::grad( int hash, float x, float y, float z ) {
	int h = hash & 15;
	return grad3lut[h][0]*x + grad3lut[h][1]*y + grad3lut[h][2]*z;
}
float SimplexNoise::grad( int hash, float x, float y, float z, float t ) {
	int h = hash & 31;
	return grad4lut[h][0]*x + grad4lut[h][1]*y + grad4lut[h][2]*z + grad4lut[h][3]*t;
}
void SimplexNoise::grad1( int hash, float *gx ) { *gx = grad(hash, 1.0f); }
void SimplexNoise::grad2( int hash, float *gx, float *gy ) { int h = hash & 7; *gx = grad2lut[h][0]; *gy = grad2lut[h][1]; }
void SimplexNoise::grad3( int hash, float *gx, float *gy, float *gz ) { int h = hash & 15; *gx = grad3lut[h][0]; *gy = grad3lut[h][1]; *gz = grad3lut[h][2]; }
void SimplexNoise::grad4( int hash, float *gx, float *gy, float *gz, float *gw ) { int h = hash & 31; *gx = grad4lut[h][0]; *gy = grad4lut[h][1]; *gz = grad4lut[h][2]; *gw = grad4lut[h][3]; }
float SimplexNoise::graddotp2( float gx, float gy, float x, float y ) { return gx * x + gy * y; }
float SimplexNoise::graddotp3( float gx, float gy, float gz, float x, float y, float z ) { return gx * x + gy * y + gz * z; }

float SimplexNoise::noise( float x ) {
    float n0, n1;
    float s = x * F2;
    int i = FASTFLOOR(x+s);
    float t = i * G2;
    float X0 = i-t;
    float x0 = x - X0;
    int i1 = (x0 > 0) ? 1 : 0;
    float x1 = x0 - i1 + G2;
    int ii = i & 255;
    int gi0 = perm[ii] % 12;
    int gi1 = perm[ii+i1] % 12;
    float t0 = 0.5f - x0*x0;
    if(t0<0) n0 = 0.0;
    else {
        t0 *= t0;
        n0 = t0 * t0 * grad(gi0, x0);
    }
    float t1 = 0.5f - x1*x1;
    if(t1<0) n1 = 0.0;
    else {
        t1 *= t1;
        n1 = t1 * t1 * grad(gi1, x1);
    }
    return 70.0f * (n0 + n1);
}
float SimplexNoise::noise( const glm::vec2 &v ) {
    float n0, n1, n2;
    float s = (v.x+v.y)*F2;
    int i = FASTFLOOR(v.x+s);
    int j = FASTFLOOR(v.y+s);
    float t = (i+j)*G2;
    float X0 = i-t;
    float Y0 = j-t;
    float x0 = v.x-X0;
    float y0 = v.y-Y0;
    int i1, j1;
    if(x0>y0) {i1=1; j1=0;}
    else {i1=0; j1=1;}
    float x1 = x0 - i1 + G2;
    float y1 = y0 - j1 + G2;
    float x2 = x0 - 1.0f + 2.0f * G2;
    float y2 = y0 - 1.0f + 2.0f * G2;
    int ii = i & 255;
    int jj = j & 255;
    int gi0 = perm[ii+perm[jj]] % 12;
    int gi1 = perm[ii+i1+perm[jj+j1]] % 12;
    int gi2 = perm[ii+1+perm[jj+1]] % 12;
    float t0 = 0.5f - x0*x0-y0*y0;
    if(t0<0) n0 = 0.0;
    else {
        t0 *= t0;
        n0 = t0 * t0 * grad(gi0, x0, y0);
    }
    float t1 = 0.5f - x1*x1-y1*y1;
    if(t1<0) n1 = 0.0;
    else {
        t1 *= t1;
        n1 = t1 * t1 * grad(gi1, x1, y1);
    }
    float t2 = 0.5f - x2*x2-y2*y2;
    if(t2<0) n2 = 0.0;
    else {
        t2 *= t2;
        n2 = t2 * t2 * grad(gi2, x2, y2);
    }
    return 70.0f * (n0 + n1 + n2);
}
float SimplexNoise::noise( const glm::vec3 &v ) {
	float n0, n1, n2, n3;
	float s = (v.x+v.y+v.z)*F3;
	int i = FASTFLOOR(v.x+s);
	int j = FASTFLOOR(v.y+s);
	int k = FASTFLOOR(v.z+s);
	float t = (i+j+k)*G3;
	float X0 = i-t;
	float Y0 = j-t;
	float Z0 = k-t;
	float x0 = v.x-X0;
	float y0 = v.y-Y0;
	float z0 = v.z-Z0;
	int i1, j1, k1;
	int i2, j2, k2;
	if(x0>=y0) {
		if(y0>=z0)
		{ i1=1; j1=0; k1=0; i2=1; j2=1; k2=0; }
		else if(x0>=z0) { i1=1; j1=0; k1=0; i2=1; j2=0; k2=1; }
		else { i1=0; j1=0; k1=1; i2=1; j2=0; k2=1; }
	}
	else {
		if(y0<z0) { i1=0; j1=0; k1=1; i2=0; j2=1; k2=1; }
		else if(x0<z0) { i1=0; j1=1; k1=0; i2=0; j2=1; k2=1; }
		else { i1=0; j1=1; k1=0; i2=1; j2=1; k2=0; }
	}
	float x1 = x0 - i1 + G3;
	float y1 = y0 - j1 + G3;
	float z1 = z0 - k1 + G3;
	float x2 = x0 - i2 + 2.0f*G3;
	float y2 = y0 - j2 + 2.0f*G3;
	float z2 = z0 - k2 + 2.0f*G3;
	float x3 = x0 - 1.0f + 3.0f*G3;
	float y3 = y0 - 1.0f + 3.0f*G3;
	float z3 = z0 - 1.0f + 3.0f*G3;
	int ii = i & 255;
	int jj = j & 255;
	int kk = k & 255;
	int gi0 = perm[ii+perm[jj+perm[kk]]] % 12;
	int gi1 = perm[ii+i1+perm[jj+j1+perm[kk+k1]]] % 12;
	int gi2 = perm[ii+i2+perm[jj+j2+perm[kk+k2]]] % 12;
	int gi3 = perm[ii+1+perm[jj+1+perm[kk+1]]] % 12;
	float t0 = 0.6f - x0*x0 - y0*y0 - z0*z0;
	if(t0<0) n0 = 0.0;
	else {
		t0 *= t0;
		n0 = t0 * t0 * grad(gi0, x0, y0, z0);
	}
	float t1 = 0.6f - x1*x1 - y1*y1 - z1*z1;
	if(t1<0) n1 = 0.0;
	else {
		t1 *= t1;
		n1 = t1 * t1 * grad(gi1, x1, y1, z1);
	}
	float t2 = 0.6f - x2*x2 - y2*y2 - z2*z2;
	if(t2<0) n2 = 0.0;
	else {
		t2 *= t2;
		n2 = t2 * t2 * grad(gi2, x2, y2, z2);
	}
	float t3 = 0.6f - x3*x3 - y3*y3 - z3*z3;
	if(t3<0) n3 = 0.0;
	else {
		t3 *= t3;
		n3 = t3 * t3 * grad(gi3, x3, y3, z3);
	}
	return 32.0f*(n0 + n1 + n2 + n3);
}
float SimplexNoise::noise( const glm::vec4 &v ) {
	float n0, n1, n2, n3, n4;
	float s  = (v.x + v.y + v.z + v.w) * F4;
	int   i  = FASTFLOOR(v.x + s);
	int   j  = FASTFLOOR(v.y + s);
	int   k  = FASTFLOOR(v.z + s);
	int   l  = FASTFLOOR(v.w + s);
	float t  = (i + j + k + l) * G4;
	float X0 = i - t;
	float Y0 = j - t;
	float Z0 = k - t;
	float W0 = l - t;
	float x0 = v.x - X0;
	float y0 = v.y - Y0;
	float z0 = v.z - Z0;
	float w0 = v.w - W0;
	int c1 = (x0 > y0) ? 32 : 0;
	int c2 = (x0 > z0) ? 16 : 0;
	int c3 = (y0 > z0) ? 8 : 0;
	int c4 = (x0 > w0) ? 4 : 0;
	int c5 = (y0 > w0) ? 2 : 0;
	int c6 = (z0 > w0) ? 1 : 0;
	int c = c1 + c2 + c3 + c4 + c5 + c6;
	int i1, j1, k1, l1;
	int i2, j2, k2, l2;
	int i3, j3, k3, l3;
	i1 = (c >> 0) & 1; j1 = (c >> 1) & 1; k1 = (c >> 2) & 1; l1 = (c >> 3) & 1;
	i2 = (c >> 4) & 1; j2 = (c >> 5) & 1; k2 = (c >> 6) & 1; l2 = (c >> 7) & 1;
	i3 = (c >> 8) & 1; j3 = (c >> 9) & 1; k3 = (c >> 10) & 1; l3 = (c >> 11) & 1;
	float x1 = x0 - i1 + G4;
	float y1 = y0 - j1 + G4;
	float z1 = z0 - k1 + G4;
	float w1 = w0 - l1 + G4;
	float x2 = x0 - i2 + 2.0f*G4;
	float y2 = y0 - j2 + 2.0f*G4;
	float z2 = z0 - k2 + 2.0f*G4;
	float w2 = w0 - l2 + 2.0f*G4;
	float x3 = x0 - i3 + 3.0f*G4;
	float y3 = y0 - j3 + 3.0f*G4;
	float z3 = z0 - k3 + 3.0f*G4;
	float w3 = w0 - l3 + 3.0f*G4;
	float x4 = x0 - 1.0f + 4.0f*G4;
	float y4 = y0 - 1.0f + 4.0f*G4;
	float z4 = z0 - 1.0f + 4.0f*G4;
	float w4 = w0 - 1.0f + 4.0f*G4;
	int ii = i & 255;
	int jj = j & 255;
	int kk = k & 255;
	int ll = l & 255;
	int gi0 = perm[ii+perm[jj+perm[kk+perm[ll]]]] % 32;
	int gi1 = perm[ii+i1+perm[jj+j1+perm[kk+k1+perm[ll+l1]]]] % 32;
	int gi2 = perm[ii+i2+perm[jj+j2+perm[kk+k2+perm[ll+l2]]]] % 32;
	int gi3 = perm[ii+i3+perm[jj+j3+perm[kk+k3+perm[ll+l3]]]] % 32;
	int gi4 = perm[ii+1+perm[jj+1+perm[kk+1+perm[ll+1]]]] % 32;
	float t0 = 0.6f - x0*x0 - y0*y0 - z0*z0 - w0*w0;
	if(t0<0) n0 = 0.0;
	else {
		t0 *= t0;
		n0 = t0 * t0 * grad(gi0, x0, y0, z0, w0);
	}
	float t1 = 0.6f - x1*x1 - y1*y1 - z1*z1 - w1*w1;
	if(t1<0) n1 = 0.0;
	else {
		t1 *= t1;
		n1 = t1 * t1 * grad(gi1, x1, y1, z1, w1);
	}
	float t2 = 0.6f - x2*x2 - y2*y2 - z2*z2 - w2*w2;
	if(t2<0) n2 = 0.0;
	else {
		t2 *= t2;
		n2 = t2 * t2 * grad(gi2, x2, y2, z2, w2);
	}
	float t3 = 0.6f - x3*x3 - y3*y3 - z3*z3 - w3*w3;
	if(t3<0) n3 = 0.0;
	else {
		t3 *= t3;
		n3 = t3 * t3 * grad(gi3, x3, y3, z3, w3);
	}
	float t4 = 0.6f - x4*x4 - y4*y4 - z4*z4 - w4*w4;
	if(t4<0) n4 = 0.0;
	else {
		t4 *= t4;
		n4 = t4 * t4 * grad(gi4, x4, y4, z4, w4);
	}
	return 27.0f * (n0 + n1 + n2 + n3 + n4);
}
glm::vec2 SimplexNoise::dnoise( float x ) {
	float n;
	float g;
	float s = x * F2;
	int i = FASTFLOOR(x+s);
	float t = i * G2;
	float X0 = i-t;
	float x0 = x - X0;
	int i1 = (x0 > 0) ? 1 : 0;
	float x1 = x0 - i1 + G2;
	int ii = i & 255;
	int gi0 = perm[ii] % 12;
	int gi1 = perm[ii+i1] % 12;
	float t0 = 0.5f - x0*x0;
	float gx0;
	if(t0<0) n = 0.0;
	else {
		t0 *= t0;
		grad1(gi0, &gx0);
		n = t0 * t0 * gx0 * x0;
	}
	float t1 = 0.5f - x1*x1;
	float gx1;
	if(t1<0) g = 0.0;
	else {
		t1 *= t1;
		grad1(gi1, &gx1);
		g = t1 * t1 * gx1 * x1;
	}
	return glm::vec2( 70.0f * (n + g), 70.0f * ( (gx0*t0*t0*-4*x0) + (gx1*t1*t1*-4*x1) ) );
}
glm::vec3 SimplexNoise::dnoise( const glm::vec2 &v ) {
	float n;
	float dv[2];
	float s = (v.x + v.y) * F2;
	float xs = v.x + s;
	float ys = v.y + s;
	int i = FASTFLOOR(xs);
	int j = FASTFLOOR(ys);
	float t = (float)(i + j) * G2;
	float X0 = i - t;
	float Y0 = j - t;
	float x0 = v.x - X0;
	float y0 = v.y - Y0;
	int i1, j1;
	if(x0 > y0) { i1=1; j1=0; }
	else { i1=0; j1=1; }
	float x1 = x0 - i1 + G2;
	float y1 = y0 - j1 + G2;
	float x2 = x0 - 1.0f + 2.0f * G2;
	float y2 = y0 - 1.0f + 2.0f * G2;
	int ii = i & 255;
	int jj = j & 255;
	int gi0 = perm[ii+perm[jj]] % 8;
	int gi1 = perm[ii+i1+perm[jj+j1]] % 8;
	int gi2 = perm[ii+1+perm[jj+1]] % 8;
	float t0 = 0.5f - x0*x0 - y0*y0;
	float t1 = 0.5f - x1*x1 - y1*y1;
	float t2 = 0.5f - x2*x2 - y2*y2;
	float gx0, gy0, gx1, gy1, gx2, gy2;
	grad2(gi0, &gx0, &gy0);
	grad2(gi1, &gx1, &gy1);
	grad2(gi2, &gx2, &gy2);
	if (t0 < 0.0f) {
		t0 = 0.0f;
		gx0 = 0.0f;
		gy0 = 0.0f;
	} else {
		t0 *= t0;
		gx0 = t0 * t0 * gx0;
		gy0 = t0 * t0 * gy0;
	}
	if (t1 < 0.0f) {
		t1 = 0.0f;
		gx1 = 0.0f;
		gy1 = 0.0f;
	} else {
		t1 *= t1;
		gx1 = t1 * t1 * gx1;
		gy1 = t1 * t1 * gy1;
	}
	if (t2 < 0.0f) {
		t2 = 0.0f;
		gx2 = 0.0f;
		gy2 = 0.0f;
	} else {
		t2 *= t2;
		gx2 = t2 * t2 * gx2;
		gy2 = t2 * t2 * gy2;
	}
	n = 40.0f * (t0*t0*graddotp2(gx0, gy0, x0, y0) + t1*t1*graddotp2(gx1, gy1, x1, y1) + t2*t2*graddotp2(gx2, gy2, x2, y2));
	dv[0] = -8.0f * (t0*gx0 + t1*gx1 + t2*gx2) + 40.0f * (t0*x0*graddotp2(gx0, gy0, x0, y0) + t1*x1*graddotp2(gx1, gy1, x1, y1) + t2*x2*graddotp2(gx2, gy2, x2, y2));
	dv[1] = -8.0f * (t0*gy0 + t1*gy1 + t2*gy2) + 40.0f * (t0*y0*graddotp2(gx0, gy0, x0, y0) + t1*y1*graddotp2(gx1, gy1, x1, y1) + t2*y2*graddotp2(gx2, gy2, x2, y2));
	return glm::vec3(n, dv[0], dv[1]);
}
glm::vec4 SimplexNoise::dnoise( const glm::vec3 &v ) {
    return glm::vec4(0);
}
SimplexNoise::vec5 SimplexNoise::dnoise( const glm::vec4 &v ) {
    return vec5();
}

// Other function implementations...
float SimplexNoise::ridgedNoise( float x ) { return 0; }
float SimplexNoise::ridgedNoise( const glm::vec2 &v ) { return 0; }
float SimplexNoise::ridgedNoise( const glm::vec3 &v ) { return 0; }
float SimplexNoise::ridgedNoise( const glm::vec4 &v ) { return 0; }
float SimplexNoise::worleyNoise( const glm::vec2 &v ) { return 0; }
float SimplexNoise::worleyNoise( const glm::vec3 &v ) { return 0; }
float SimplexNoise::worleyNoise( const glm::vec2 &v, float falloff ) { return 0; }
float SimplexNoise::worleyNoise( const glm::vec3 &v, float falloff ) { return 0; }
float SimplexNoise::flowNoise( const glm::vec2 &v, float angle ) { return 0; }
float SimplexNoise::flowNoise( const glm::vec3 &v, float angle ) { return 0; }
glm::vec3 SimplexNoise::dFlowNoise( const glm::vec2 &v, float angle ) { return glm::vec3(0); }
glm::vec4 SimplexNoise::dFlowNoise( const glm::vec3 &v, float angle ) { return glm::vec4(0); }
glm::vec2 SimplexNoise::curlNoise( const glm::vec2 &v ) { return glm::vec2(0); }
glm::vec2 SimplexNoise::curlNoise( const glm::vec2 &v, float t ) { return glm::vec2(0); }
glm::vec2 SimplexNoise::curlNoise( const glm::vec2 &v, uint8_t octaves, float lacunarity, float gain ) { return glm::vec2(0); }
glm::vec3 SimplexNoise::curlNoise( const glm::vec3 &v ) { return glm::vec3(0); }
glm::vec3 SimplexNoise::curlNoise( const glm::vec3 &v, float t ) { return glm::vec3(0); }
glm::vec3 SimplexNoise::curlNoise( const glm::vec3 &v, uint8_t octaves, float lacunarity, float gain ) { return glm::vec3(0); }
glm::vec2 SimplexNoise::curl( const glm::vec2 &v, const std::function<float(const glm::vec2&)> &potential, float delta ) { return glm::vec2(0); }
glm::vec3 SimplexNoise::curl( const glm::vec3 &v, const std::function<glm::vec3(const glm::vec3&)> &potential, float delta ) { return glm::vec3(0); }
float SimplexNoise::fBm( float x, uint8_t octaves, float lacunarity, float gain ) { return 0; }
float SimplexNoise::fBm( const glm::vec2 &v, uint8_t octaves, float lacunarity, float gain ) { return 0; }
float SimplexNoise::fBm( const glm::vec3 &v, uint8_t octaves, float lacunarity, float gain ) { return 0; }
float SimplexNoise::fBm( const glm::vec4 &v, uint8_t octaves, float lacunarity, float gain ) { return 0; }
float SimplexNoise::worleyfBm( const glm::vec2 &v, uint8_t octaves, float lacunarity, float gain ) { return 0; }
float SimplexNoise::worleyfBm( const glm::vec3 &v, uint8_t octaves, float lacunarity, float gain ) { return 0; }
float SimplexNoise::worleyfBm( const glm::vec2 &v, float falloff, uint8_t octaves, float lacunarity, float gain ) { return 0; }
float SimplexNoise::worleyfBm( const glm::vec3 &v, float falloff, uint8_t octaves, float lacunarity, float gain ) { return 0; }
glm::vec2 SimplexNoise::dfBm( float x, uint8_t octaves, float lacunarity, float gain ) { return glm::vec2(0); }
glm::vec3 SimplexNoise::dfBm( const glm::vec2 &v, uint8_t octaves, float lacunarity, float gain ) { return glm::vec3(0); }
glm::vec4 SimplexNoise::dfBm( const glm::vec3 &v, uint8_t octaves, float lacunarity, float gain ) { return glm::vec4(0); }
SimplexNoise::vec5 SimplexNoise::dfBm( const glm::vec4 &v, uint8_t octaves, float lacunarity, float gain ) { return vec5(); }
float SimplexNoise::ridgedMF( float x, float ridgeOffset, uint8_t octaves, float lacunarity, float gain ) { return 0; }
float SimplexNoise::ridgedMF( const glm::vec2 &v, float ridgeOffset, uint8_t octaves, float lacunarity, float gain ) { return 0; }
float SimplexNoise::ridgedMF( const glm::vec3 &v, float ridgeOffset, uint8_t octaves, float lacunarity, float gain ) { return 0; }
float SimplexNoise::ridgedMF( const glm::vec4 &v, float ridgeOffset, uint8_t octaves, float lacunarity, float gain ) { return 0; }
float SimplexNoise::iqfBm( const glm::vec2 &v, uint8_t octaves, float lacunarity, float gain ) { return 0; }
float SimplexNoise::iqfBm( const glm::vec3 &v, uint8_t octaves, float lacunarity, float gain ) { return 0; }
float SimplexNoise::iqMatfBm( const glm::vec2 &v, uint8_t octaves, const glm::mat2 &mat, float gain ) { return 0; }
