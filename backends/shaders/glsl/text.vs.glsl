#version 450

layout(push_constant) uniform PushConstants {
    vec2 screenSize;
} pc;

struct CharacterInstance {
    vec2 position;
    vec2 size;
    uint glyphIndex;
    uint color;
    uint strokeColor;
    float strokeThickness;
};

layout(std430, binding = 0) readonly buffer CharacterBuffer {
    CharacterInstance characters[];
};

layout(location = 0) out vec2 fragUV;
layout(location = 1) out flat uint fragGlyphIndex;
layout(location = 2) out flat vec4 fragColor;
layout(location = 3) out flat vec2 fragSize;
layout(location = 4) out flat float fragStrokeThickness;
layout(location = 5) out flat vec4 fragStrokeColor;

const vec2 positions[4] = vec2[](
    vec2(0.0, 0.0),
    vec2(1.0, 0.0),
    vec2(1.0, 1.0),
    vec2(0.0, 1.0)
);

vec4 unpackColor(uint packed) {
    return vec4(
        float((packed >> 0) & 0xFF) / 255.0,
        float((packed >> 8) & 0xFF) / 255.0,
        float((packed >> 16) & 0xFF) / 255.0,
        float((packed >> 24) & 0xFF) / 255.0
    );
}

void main()
{
    CharacterInstance ch = characters[gl_InstanceIndex];

    vec2 localPos = positions[gl_VertexIndex];
    vec2 worldPos = ch.position + localPos * ch.size;

    vec2 ndc = (worldPos / pc.screenSize) * 2.0 - 1.0;
    gl_Position = vec4(ndc, 0.0, 1.0);

    fragUV = localPos;
    fragGlyphIndex = ch.glyphIndex;
    fragColor = unpackColor(ch.color);
    fragSize = ch.size;
    fragStrokeThickness = ch.strokeThickness;
    fragStrokeColor = unpackColor(ch.strokeColor);
}
