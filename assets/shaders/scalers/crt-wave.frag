/*
 * crt-wave.frag - a fragment shader that generates scanlines with a sine wave
 *                 based on semisoft.frag
 * -- David (astral) Cravens 2017 (decravens@gmail.com)
 *
 * settings:
 *     DARKEN         - how dark the gaps get
 *     BOOST          - how bright the scanlines are
 *     SCAN_FREQUENCY - how rapidly down Y the wave oscillates
 */
#version 110
uniform sampler2D baseMap;
varying vec2 vTexcoord;

#define XS 1024.0
#define YS 512.0
#define AX 0.5/XS
#define AY 0.5/YS

#define TS vec2(XS, YS)

// -- settings ---------------
#define DARKEN 0.8
#define BOOST 1.4
#define SCAN_FREQUENCY 3.0
// ---------------------------

#define HALFERS ((BOOST - DARKEN) * 0.5)

float calc_scanline( float y )
{
    return DARKEN + HALFERS + sin(2.0*3.14*y/SCAN_FREQUENCY) * HALFERS;
}

void main( void )
{
    vec2 tcbase = (floor(vTexcoord * TS + 0.5) + 0.5) / TS;
    vec2 tcdiff = vTexcoord - tcbase;
    vec2 sdiff = sign(tcdiff);
    vec2 adiff = pow(abs(tcdiff) * TS, vec2(3.0));

    gl_FragColor = texture2D(baseMap, tcbase + sdiff * adiff / TS) * calc_scanline(gl_FragCoord.y);
}