#version 330 core

in vec4 vColor;
in float vEdge;
in vec2 vPos;

// Gradient uniforms
uniform int uPaintType;       // 0=solid (vertex color), 1=linear gradient, 2=radial gradient
uniform vec2 uGradStart;      // gradient start point (screen-space)
uniform vec2 uGradEnd;        // gradient end point (screen-space)
uniform float uGradInnerR;    // radial: inner radius
uniform float uGradOuterR;    // radial: outer radius
uniform int uGradStopCount;   // number of gradient stops (max 8)
uniform float uGradPositions[8];
uniform vec4 uGradColors[8];

out vec4 fragColor;

vec4 sampleGradient(float t) {
    t = clamp(t, 0.0, 1.0);
    if (uGradStopCount <= 0) return vColor;
    if (uGradStopCount == 1) return uGradColors[0];

    // Find the two stops that bracket t
    if (t <= uGradPositions[0]) return uGradColors[0];
    for (int i = 1; i < uGradStopCount; ++i) {
        if (t <= uGradPositions[i]) {
            float localT = (t - uGradPositions[i-1]) / (uGradPositions[i] - uGradPositions[i-1]);
            return mix(uGradColors[i-1], uGradColors[i], localT);
        }
    }
    return uGradColors[uGradStopCount - 1];
}

void main() {
    vec4 color;

    if (uPaintType == 1) {
        // Linear gradient
        vec2 gradDir = uGradEnd - uGradStart;
        float gradLen = length(gradDir);
        float t = 0.0;
        if (gradLen > 0.001) {
            t = dot(vPos - uGradStart, gradDir) / (gradLen * gradLen);
        }
        color = sampleGradient(t);
        color.a *= vColor.a; // preserve per-vertex alpha for opacity
    } else if (uPaintType == 2) {
        // Radial gradient
        float dist = length(vPos - uGradStart);
        float range = uGradOuterR - uGradInnerR;
        float t = 0.0;
        if (range > 0.001) {
            t = (dist - uGradInnerR) / range;
        }
        color = sampleGradient(t);
        color.a *= vColor.a;
    } else {
        // Solid (vertex color)
        color = vColor;
    }

    float alpha = smoothstep(0.0, 1.0, vEdge);
    fragColor = vec4(color.rgb, color.a * alpha);
}