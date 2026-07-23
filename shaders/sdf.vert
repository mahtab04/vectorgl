#version 330 core

layout(location=0) in vec2 aQuadPos;

layout(location=1) in vec2 iPos;
layout(location=2) in vec2 iSize;
layout(location=3) in vec4 iCornerRadii;
layout(location=4) in vec4 iFillColor;
layout(location=5) in vec4 iStrokeColor;
layout(location=6) in vec4 iParams;

uniform vec2 uViewSize;

out vec2 vLocalPos;
out vec2 vHalfSize;
out vec4 vCornerRadii;
out vec4 vFillColor;
out vec4 vStrokeColor;
out float vStrokeWidth;
out float vOpacity;
out float vShapeType;

void main() {
    float rotation = iParams.y;
    float c = cos(rotation), s = sin(rotation);
    mat2 rot = mat2(c, s, -s, c);

    float padding = iParams.x + 2.0;
    vec2 expandedSize = iSize + vec2(padding);
    vec2 localPos = aQuadPos * expandedSize;
    vec2 worldPos = iPos + rot * localPos;

    vec2 ndc = (worldPos / uViewSize) * 2.0 - 1.0;
    ndc.y = -ndc.y;
    gl_Position = vec4(ndc, 0.0, 1.0);

    vLocalPos = aQuadPos * expandedSize;
    vHalfSize = iSize;
    vCornerRadii = iCornerRadii;
    vFillColor = iFillColor;
    vStrokeColor = iStrokeColor;
    vStrokeWidth = iParams.x;
    vOpacity = iParams.z;
    vShapeType = iParams.w;
}