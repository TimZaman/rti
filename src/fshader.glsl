#ifdef GL_ES
// Set default precision to medium
precision mediump int;
precision mediump float;
#endif

uniform highp float weights[6];
uniform highp int biases[6];
uniform highp float scales[6];

uniform sampler2D texture[6];

varying vec2 v_texcoord; //EXAMPLE
//varying highp vec4 texc;

//! [0]
void main()
{



	// Set fragment color from texture
	// gl_FragColor = texture2D(texture[1], v_texcoord) * weights[0];
	highp vec4 color =  texture2D(texture[0], v_texcoord) * weights[0];
	           color += texture2D(texture[1], v_texcoord) * weights[1];
	           color += texture2D(texture[2], v_texcoord) * weights[2];
	           color += texture2D(texture[3], v_texcoord) * weights[3];
	           color += texture2D(texture[4], v_texcoord) * weights[4];
	           color += texture2D(texture[5], v_texcoord); //weight[5] is a constant (1)
	color.r = color.r < 0.0 ? 0.0 : color.r > 1.0 ? 1.0 : color.r;
	color.g = color.g < 0.0 ? 0.0 : color.g > 1.0 ? 1.0 : color.g;
	color.b = color.b < 0.0 ? 0.0 : color.b > 1.0 ? 1.0 : color.b;
	color.a = 1.0;

	gl_FragColor = color;

}
//! [0]