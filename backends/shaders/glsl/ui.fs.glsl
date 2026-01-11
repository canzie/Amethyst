#version 450

layout(location = 0) in vec2 fragUV;
layout(location = 1) in flat uint fragPrimitiveType;
layout(location = 2) in flat vec3 fragFillColor;
layout(location = 3) in flat vec3 fragBorderColor;
layout(location = 4) in flat float fragBorderThickness;
layout(location = 5) in flat float fragCornerRadius;
layout(location = 6) in flat vec2 fragSize;

layout(location = 0) out vec4 outColor;

const uint PRIMITIVE_RECT = 0;
const uint PRIMITIVE_CIRCLE = 1;
const uint PRIMITIVE_TRIANGLE = 2;
const uint PRIMITIVE_LINE = 3;


float sdfRect(vec2 p, vec2 halfSize, float radius)
{
    vec2 d = abs(p) - (halfSize - vec2(radius)); // subtract radius from halfSize
    vec2 dMax = max(d, 0.0);
    return length(dMax) + min(max(d.x, d.y), 0.0) - radius;
}

float sdfCircle(vec2 p, float radius)
{
    return length(p) - radius;
}

float sdfTriangle(vec2 p, vec2 halfSize)
{
    p.x = abs(p.x);
    vec2 a = vec2(halfSize.x, -halfSize.y);
    vec2 b = vec2(0.0, halfSize.y);
    vec2 ba = b - a;
    vec2 pa = p - a;
    float h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
    float d = length(pa - ba * h);
    float s = sign(pa.x * ba.y - pa.y * ba.x);
    return d * s;
}

float sdfLine(vec2 p, vec2 halfSize, float thickness)
{
    float d = abs(p.y) - thickness * 0.5;
    d = max(d, abs(p.x) - halfSize.x);
    return d;
}

void main()
{
    vec2 p = (fragUV - 0.5) * fragSize;
    vec2 halfSize = fragSize * 0.5;

    float dist;

    if (fragPrimitiveType == PRIMITIVE_RECT) {
        dist = sdfRect(p, halfSize, fragCornerRadius);
    } else if (fragPrimitiveType == PRIMITIVE_CIRCLE) {
        dist = sdfCircle(p, min(halfSize.x, halfSize.y));
    } else if (fragPrimitiveType == PRIMITIVE_TRIANGLE) {
        dist = sdfTriangle(p, halfSize);
    } else if (fragPrimitiveType == PRIMITIVE_LINE) {
        dist = sdfLine(p, halfSize, fragCornerRadius);
    } else {
        dist = sdfRect(p, halfSize, 0.0);
    }

    float aa = fwidth(dist);
    float alpha = 1.0 - smoothstep(-aa, aa, dist);

    vec3 color = fragFillColor;
    if (fragBorderThickness > 0.0) {
        float borderDist = abs(dist) - fragBorderThickness;
        float borderAlpha = 1.0 - smoothstep(-aa, aa, borderDist);
        color = mix(fragFillColor, fragBorderColor, borderAlpha * step(dist, 0.0));
    }

    if (alpha < 0.001) discard;

    outColor = vec4(color, alpha);
}
