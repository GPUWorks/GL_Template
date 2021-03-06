#version 330

// Output: UV coordinates
out INTERFACE {
	vec2 uv;
} Out ; ///< vec2 uv;

uniform float screenRatio;
uniform float imageRatio;
uniform float widthRatio;
uniform bool isHDR;
uniform vec2 flipAxis;
uniform vec2 angleTrig;
uniform float pixelScale;
uniform vec2 mouseShift;

/**
 Generate one triangle covering the whole screen
 2: (-1,3),(0,2)
 *
 | \
 |	 \
 |	   \
 |		 \
 |		   \
 *-----------*  1: (3,-1), (2,0)
 0: (-1,-1), (0,0)
 */
void main(){
	vec2 temp = 2.0 * vec2(gl_VertexID == 1, gl_VertexID == 2);
	gl_Position.xy = 2.0 * temp - 1.0;
	gl_Position.zw = vec2(1.0);
	// Center uvs and scale/translate.
	vec2 uv = pixelScale * gl_Position.xy - mouseShift*vec2(2.0,-2.0);
	
	// Image and screen ratio corrections.
	float HDRflip = (isHDR ? -1.0 : 1.0);
	uv *= vec2(imageRatio, HDRflip*screenRatio);
	uv *= widthRatio;
	
	// Rotation.
	float nx = angleTrig.x*uv.x-HDRflip*angleTrig.y*uv.y;
	float ny = angleTrig.x*uv.y+HDRflip*angleTrig.y*uv.x;
	uv = vec2(nx,ny);
	
	// Flipping
	uv  *= 1.0 - flipAxis * 2.0;
	Out.uv = uv * 0.5 + 0.5;
}
