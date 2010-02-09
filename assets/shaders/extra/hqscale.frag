uniform sampler2D baseMap;

varying vec2 vTexcoord;
#define XS 1024.0
#define YS 512.0
#define AX1 -0.5/XS
#define AY1 -0.5/YS
#define AX2 0.5/XS
#define AY2 0.5/YS
#define POW 7.5
#define PADD 0.0000002
#define XBLUR1 -0.5/XS
#define YBLUR1 -0.5/YS
#define XBLUR2 0.5/XS
#define YBLUR2 0.5/YS
#define XBLUR3 -.5/XS
#define YBLUR3 -.5/YS
#define XBLUR4 .5/XS
#define YBLUR4 .5/YS
#define F1 8.0
#define F2 0.0
#define F3 0.0
#define F4 0.0
#define F5 7.0
#define F6 0.0
#define F7 0.0
#define F8 -2.0
#define F9 0.0
#define FT 13.0
void main( void )
{
   
   vec4 c = (texture2D( baseMap, vTexcoord+vec2(((XBLUR1+XBLUR2)/2.0), (YBLUR1+YBLUR2)/2.0)) *F5 + 
               /*texture2D( baseMap, vTexcoord+vec2(((XBLUR1+XBLUR2)/2.0), YBLUR1))              *F2 +*/ 
             texture2D( baseMap, vTexcoord+vec2(((XBLUR1+XBLUR2)/2.0), YBLUR2))              *F8 + 
               /*texture2D( baseMap, vTexcoord+vec2(XBLUR1,                (YBLUR1+YBLUR2)/2.0)) *F4 +*/
               /*texture2D( baseMap, vTexcoord+vec2(XBLUR2,                (YBLUR1+YBLUR2)/2.0)) *F6 +*/
             texture2D( baseMap, vTexcoord+vec2(XBLUR3,                YBLUR3))              *F1
               /*texture2D( baseMap, vTexcoord+vec2(XBLUR4,                YBLUR3))              *F3 +*/
               /*texture2D( baseMap, vTexcoord+vec2(XBLUR3,                YBLUR4))              *F7 +*/
               /*texture2D( baseMap, vTexcoord+vec2(XBLUR4,                YBLUR4))              *F9  */
													)/FT;
   float d1, d2, d3, d4, dt, w1, w2, w3, w4, wt;
   vec4 final;
   vec4 c1 = texture2D( baseMap, vec2(floor(vTexcoord.x*XS)/XS+AX1, 
floor(vTexcoord.y*YS)/YS+AY1));
   vec4 c2 = texture2D( baseMap, vec2(floor(vTexcoord.x*XS)/XS+AX2, 
floor(vTexcoord.y*YS)/YS+AY1));
   vec4 c3 = texture2D( baseMap, vec2(floor(vTexcoord.x*XS)/XS+AX1, 
floor(vTexcoord.y*YS)/YS+AY2));
   vec4 c4 = texture2D( baseMap, vec2(floor(vTexcoord.x*XS)/XS+AX2, 
floor(vTexcoord.y*YS)/YS+AY2));
   d1 = (c1.x - c.x)*(c1.x - c.x) + (c1.y - c.y)*(c1.y - c.y) + (c1.z - c.z)*(c1.z - c.z);
   d2 = (c2.x - c.x)*(c2.x - c.x) + (c2.y - c.y)*(c2.y - c.y) + (c2.z - c.z)*(c2.z - c.z);
   d3 = (c3.x - c.x)*(c3.x - c.x) + (c3.y - c.y)*(c3.y - c.y) + (c3.z - c.z)*(c3.z - c.z);
   d4 = (c4.x - c.x)*(c4.x - c.x) + (c4.y - c.y)*(c4.y - c.y) + (c4.z - c.z)*(c4.z - c.z);
   dt = d1+d2+d3+d4;
   w1 = pow(dt-d1, POW)+PADD;
   w2 = pow(dt-d2, POW)+PADD;
   w3 = pow(dt-d3, POW)+PADD;
   w4 = pow(dt-d4, POW)+PADD;
   wt = w1+w2+w3+w4;
   final = ((c1*w1 + c2*w2 + c3*w3 + c4*w4) / wt);
   gl_FragColor = final;

}
