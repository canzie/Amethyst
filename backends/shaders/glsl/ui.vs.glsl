#version 450

layout(push_constant) uniform PushConstants {
    vec2 screenSize;
} pc;

struct InstanceData {
    mat4 transform;
    vec3 fillColor;
    float _pad0;
    vec3 borderColor;
    float _pad1;
    float borderThickness;
    float cornerRadius;
    uint primitiveType;
};

layout(std430, binding = 0) readonly buffer InstanceBuffer {
    InstanceData instances[];
};

layout(location = 0) out vec2 fragUV;
layout(location = 1) out flat uint fragPrimitiveType;
layout(location = 2) out flat vec3 fragFillColor;
layout(location = 3) out flat vec3 fragBorderColor;
layout(location = 4) out flat float fragBorderThickness;
layout(location = 5) out flat float fragCornerRadius;
layout(location = 6) out flat vec2 fragSize;

const vec2 positions[4] = vec2[](
    vec2(-0.5, -0.5),
    vec2( 0.5, -0.5),
    vec2( 0.5,  0.5),
    vec2(-0.5,  0.5)
);

const vec2 uvs[4] = vec2[](
    vec2(0.0, 0.0),
    vec2(1.0, 0.0),
    vec2(1.0, 1.0),
    vec2(0.0, 1.0)
);

void main()
{
    InstanceData inst = instances[gl_InstanceIndex];

    vec2 localPos = positions[gl_VertexIndex];
    vec4 worldPos = inst.transform * vec4(localPos, 0.0, 1.0);

    vec2 ndc = (worldPos.xy / pc.screenSize) * 2.0 - 1.0;
    gl_Position = vec4(ndc, 0.0, 1.0);

    fragUV = uvs[gl_VertexIndex];
    fragPrimitiveType = inst.primitiveType;
    fragFillColor = inst.fillColor;
    fragBorderColor = inst.borderColor;
    fragBorderThickness = inst.borderThickness;
    fragCornerRadius = inst.cornerRadius;
    fragSize = vec2(length(inst.transform[0].xy), length(inst.transform[1].xy));
}
