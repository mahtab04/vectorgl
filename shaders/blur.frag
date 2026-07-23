#version 330 core

in vec2 vUV;

uniform sampler2D uTexture;
uniform vec2 uDirection;
uniform float uRadius;

out vec4 fragColor;

void main() {
    vec2 texSize = vec2(textureSize(uTexture, 0));
    vec2 texelSize = 1.0 / texSize;
    vec4 result = vec4(0.0);
    float totalWeight = 0.0;
    int samples = int(ceil(uRadius * 2.0));
    samples = min(samples, 32);
    for (int i = -samples; i <= samples; i++) {
        float offset = float(i);
        float weight = exp(-0.5 * (offset * offset) / (uRadius * uRadius));
        vec2 tc = vUV + uDirection * texelSize * offset;
        result += texture(uTexture, tc) * weight;
        totalWeight += weight;
    }
    fragColor = result / totalWeight;
}