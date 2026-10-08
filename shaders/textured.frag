#version 330 core

in vec2 vUV;
in vec4 vColor;

uniform sampler2D uTexture;
uniform int uSDF;
uniform int uEffectTexture;

out vec4 fragColor;

void main() {
    // Captured framebuffers have bottom-left UVs and premultiplied RGB.
    vec2 uv = uEffectTexture == 1 ? vec2(vUV.x, 1.0 - vUV.y) : vUV;
    vec4 texel = texture(uTexture, uv);
    if (uSDF == 1) {
        float d = texel.r;
        // One framebuffer pixel of coverage, rather than a two-pixel blur band.
        // Match the on-edge value used by stbtt_GetGlyphSDF (128/255).
        float smoothing = max(0.5 * fwidth(d), 1.0 / 255.0);
        float edge = 128.0 / 255.0;
        float alpha = smoothstep(edge - smoothing, edge + smoothing, d);
        fragColor = vec4(vColor.rgb, vColor.a * alpha);
    } else {
        if (uEffectTexture == 1 && texel.a > 0.0)
            texel.rgb /= texel.a;
        fragColor = texel * vColor;
    }
}