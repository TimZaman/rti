#ifdef GL_ES
// Set default precision to medium
precision mediump int;
precision mediump float;
#endif

uniform mat4 mvp_matrix; //EXAMPLE

attribute vec4 a_position; //EXAMPLE


attribute vec2 a_texcoord; //EXAMPLE
varying vec2 v_texcoord; //EXAMPLE


//attribute highp vec4 vertex; //RTI
//uniform mediump mat4 matrix; //RTI

//attribute highp vec4 texCoord; //RTI
//varying highp vec4 texc; //RTI

//! [0]
void main()
{
    // Calculate vertex position in screen space
    gl_Position = mvp_matrix * a_position;  //EXAMPLE

    // Pass texture coordinate to fragment shader
    // Value will be automatically interpolated to fragments inside polygon faces
    v_texcoord = a_texcoord; //EXAMPLE
   // texc = texCoord;  //RTI
}
//! [0]