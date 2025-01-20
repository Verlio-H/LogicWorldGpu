#version 330 core
out vec4 FragColor;

uniform int sizex;
uniform int sizey;
uniform sampler2D pixels;

void main() {
    FragColor = texture(pixels, vec2(float(gl_FragCoord.x)/sizex,float(gl_FragCoord.y)/sizey),0);
}