#version 450

layout(location = 0) in vec2 fragUV;
layout(location = 1) in flat uint fragGlyphIndex;
layout(location = 2) in flat vec4 fragColor;
layout(location = 3) in flat vec2 fragSize;

layout(location = 0) out vec4 outColor;

struct Point {
    vec2 pos;
    uint flags;
};

struct Contour {
    uint pointStart;
    uint pointCount;
};

struct Glyph {
    uint contourStart;
    uint contourCount;
    uint flags;
    uint _pad0;
    vec2 bboxMin;
    vec2 bboxMax;
    float advanceWidth;
    float leftSideBearing;
    vec2 _pad1;
};

layout(std430, binding = 1) readonly buffer FontDataBuffer {
    uint pointsOffset;
    uint contoursOffset;
    uint glyphsOffset;
    uint _pad;
    // followed by: Point[], Contour[], Glyph[]
    uint data[];
};

Point getPoint(uint index) {
    uint base = pointsOffset + index * 3;
    Point p;
    p.pos.x = uintBitsToFloat(data[base + 0]);
    p.pos.y = uintBitsToFloat(data[base + 1]);
    p.flags = data[base + 2];
    return p;
}

Contour getContour(uint index) {
    uint base = contoursOffset + index * 2;
    Contour c;
    c.pointStart = data[base + 0];
    c.pointCount = data[base + 1];
    return c;
}

Glyph getGlyph(uint index) {
    uint base = glyphsOffset + index * 12;
    Glyph g;
    g.contourStart = data[base + 0];
    g.contourCount = data[base + 1];
    g.flags = data[base + 2];
    g._pad0 = data[base + 3];
    g.bboxMin.x = uintBitsToFloat(data[base + 4]);
    g.bboxMin.y = uintBitsToFloat(data[base + 5]);
    g.bboxMax.x = uintBitsToFloat(data[base + 6]);
    g.bboxMax.y = uintBitsToFloat(data[base + 7]);
    g.advanceWidth = uintBitsToFloat(data[base + 8]);
    g.leftSideBearing = uintBitsToFloat(data[base + 9]);
    return g;
}

float distanceToLineSegment(vec2 p, vec2 a, vec2 b) {
    vec2 ab = b - a;
    vec2 ap = p - a;
    float t = clamp(dot(ap, ab) / dot(ab, ab), 0.0, 1.0);
    vec2 closest = a + t * ab;
    return length(p - closest);
}

// Evaluate quadratic bezier at t: B(t) = (1-t)²·p0 + 2(1-t)t·p1 + t²·p2
vec2 evalBezier(vec2 p0, vec2 p1, vec2 p2, float t) {
    float mt = 1.0 - t;
    return mt * mt * p0 + 2.0 * mt * t * p1 + t * t * p2;
}

// Distance to quadratic bezier curve (iterative approach)
float distanceToBezier(vec2 p, vec2 p0, vec2 p1, vec2 p2) {
    // Sample the curve and find minimum distance
    float minDist = 1e10;
    const int SAMPLES = 8;

    for (int i = 0; i <= SAMPLES; i++) {
        float t = float(i) / float(SAMPLES);
        vec2 pt = evalBezier(p0, p1, p2, t);
        minDist = min(minDist, length(p - pt));
    }

    // Refine with a few Newton iterations around the best sample
    // This gives better accuracy without too many samples
    float bestT = 0.0;
    float bestDist = length(p - p0);
    for (int i = 0; i <= SAMPLES; i++) {
        float t = float(i) / float(SAMPLES);
        vec2 pt = evalBezier(p0, p1, p2, t);
        float d = length(p - pt);
        if (d < bestDist) {
            bestDist = d;
            bestT = t;
        }
    }

    // Newton-Raphson refinement
    for (int iter = 0; iter < 3; iter++) {
        float t = bestT;
        vec2 b = evalBezier(p0, p1, p2, t);
        vec2 d1 = 2.0 * ((1.0 - t) * (p1 - p0) + t * (p2 - p1)); // first derivative
        vec2 d2 = 2.0 * (p2 - 2.0 * p1 + p0); // second derivative

        vec2 diff = b - p;
        float num = dot(diff, d1);
        float den = dot(d1, d1) + dot(diff, d2);

        if (abs(den) > 1e-6) {
            bestT = clamp(t - num / den, 0.0, 1.0);
        }
    }

    return length(p - evalBezier(p0, p1, p2, bestT));
}

void main()
{
    Glyph glyph = getGlyph(fragGlyphIndex);

    if (glyph.contourCount == 0) {
        discard;
    }

    // Map UV to glyph space (flip Y since font Y is up, screen Y is down)
    vec2 glyphSize = glyph.bboxMax - glyph.bboxMin;
    vec2 uv = vec2(fragUV.x, 1.0 - fragUV.y);
    vec2 p = glyph.bboxMin + uv * glyphSize;

    float minDist = 1e10;

    // Iterate contours - all segments are beziers (on-curve, off-curve, on-curve)
    for (uint c = 0; c < glyph.contourCount; c++) {
        Contour contour = getContour(glyph.contourStart + c);

        if (contour.pointCount < 3) continue;

        // Step by 2: each bezier is points[i], points[i+1], points[i+2]
        uint numBeziers = contour.pointCount / 2;
        for (uint i = 0; i < numBeziers; i++) {
            uint idx = contour.pointStart + i * 2;
            Point p0 = getPoint(idx);
            Point p1 = getPoint(idx + 1);
            Point p2 = getPoint((i == numBeziers - 1) ? contour.pointStart : idx + 2);

            float d = distanceToBezier(p, p0.pos, p1.pos, p2.pos);
            minDist = min(minDist, d);
        }
    }

    // Scale distance to pixel space
    float pixelDist = minDist * max(fragSize.x / glyphSize.x, fragSize.y / glyphSize.y);

    // Outline thickness
    float thickness = 1.5;
    float aa = fwidth(pixelDist);
    float alpha = 1.0 - smoothstep(thickness - aa, thickness + aa, pixelDist);

    if (alpha < 0.01) discard;

    outColor = vec4(fragColor.rgb, fragColor.a * alpha);
}
