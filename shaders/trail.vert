#version 300 es
layout(location=0) in vec3 aPos;
layout(location=1) in float aAge;

uniform mat4 uViewProj;

out float vAge;

void main() {
    vAge = aAge;
    gl_Position = uViewProj * vec4(aPos, 1.0);
}
