#version 330 core

layout(location=0) in vec2 aPos;
layout(location=1) in vec4 aColor;
layout(location=2) in float aEdge;

uniform vec2 uViewSize;

out vec4 vColor;
out float vEdge;
out vec2 vPos; // screen-space position for gradient evaluation

void main() {
    vec2 pos = (aPos / uViewSize) * 2.0 - 1.0;
    pos.y = -pos.y;
    gl_Position = vec4(pos, 0.0, 1.0);
    vColor = aColor;
    vEdge = aEdge;
    vPos = aPos;
}