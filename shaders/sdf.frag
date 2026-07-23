#version 330 core

in vec2 vLocalPos;
in vec2 vHalfSize;
in vec4 vCornerRadii;
in vec4 vFillColor;
in vec4 vStrokeColor;
in float vStrokeWidth;
in float vOpacity;
in float vShapeType;

out vec4 fragColor;

float sdRoundedBox(vec2 p, vec2 b, vec4 r) {
    r.xy = (p.x > 0.0) ? r.xy : r.zw;
    r.x = (p.y > 0.0) ? r.x : r.y;
    vec2 q = abs(p) - b + r.x;
    return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - r.x;
}

float sdCircle(vec2 p, float r) {
    return length(p) - r;
}

float sdEllipse(vec2 p, vec2 r) {
    float k0 = length(p / r);
    float k1 = length(p / (r * r));
    return k0 * (k0 - 1.0) / k1;
}

void main() {
    float dist;
    int shape = int(vShapeType + 0.5);

    if (shape == 1) {
        dist = sdCircle(vLocalPos, vHalfSize.x);
    } else if (shape == 2) {
        dist = sdEllipse(vLocalPos, vHalfSize);
    } else {
        dist = sdRoundedBox(vLocalPos, vHalfSize, vCornerRadii);
    }

    float aa = fwidth(dist) * 1.0;
    float fillAlpha = 1.0 - smoothstep(-aa, aa, dist);
    vec4 color = vFillColor * fillAlpha;

    if (vStrokeWidth > 0.0) {
        float strokeDist = abs(dist) - vStrokeWidth * 0.5;
        float strokeAlpha = 1.0 - smoothstep(-aa, aa, strokeDist);
        color = mix(color, vec4(vStrokeColor.rgb, vStrokeColor.a * strokeAlpha), strokeAlpha);
    }

    color.a *= vOpacity;
    if (color.a < 0.001) discard;
    fragColor = color;
}