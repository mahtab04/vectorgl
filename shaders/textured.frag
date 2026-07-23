#version 330 core

in vec2 vUV;
in vec4 vColor;

uniform sampler2D uTexture;
uniform int uSDF;

out vec4 fragColor;

void main() {
    vec4 texel = texture(uTexture, vUV);
    if (uSDF == 1) {
        float d = texel.r;
        float smoothing = clamp(fwidth(d), 0.001, 0.1);
        float alpha = smoothstep(0.5 - smoothing, 0.5 + smoothing, d);
        fragColor = vec4(vColor.rgb, vColor.a * alpha);
    } else {
        fragColor = texel * vColor;
    }
}