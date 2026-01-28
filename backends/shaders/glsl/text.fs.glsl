#version 450

layout(location = 0) in vec2 fragUV;
layout(location = 1) in flat uint fragGlyphIndex;
layout(location = 2) in flat vec4 fragColor;
layout(location = 3) in flat vec2 fragSize;
layout(location = 4) in flat float fragStrokeThickness;
layout(location = 5) in flat vec4 fragStrokeColor;

layout(location = 0, index = 0) out vec4 outColor;
layout(location = 0, index = 1) out vec4 outBlendFactor;

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

// Compute winding contribution from a quadratic bezier for horizontal ray from p to +X
// Uses endpoint y-comparison to determine crossing direction (more robust than dy derivative)
int bezierWindingContribution(vec2 p, vec2 p0, vec2 p1, vec2 p2) {
    float y0 = p0.y - p.y;
    float y1 = p1.y - p.y;
    float y2 = p2.y - p.y;

    // Quick reject: if all control points are on same side of ray, no crossing
    if (y0 > 0.0 && y1 > 0.0 && y2 > 0.0) return 0;
    if (y0 < 0.0 && y1 < 0.0 && y2 < 0.0) return 0;

    // Solve B(t).y = p.y: a·t² + b·t + c = 0
    float a = y0 - 2.0 * y1 + y2;
    float b = 2.0 * (y1 - y0);
    float c = y0;

    int winding = 0;

    if (abs(a) < 1e-7) {
        // Linear case
        if (abs(b) > 1e-7) {
            float t = -c / b;
            if (t > 0.0 && t <= 1.0) {
                vec2 pt = evalBezier(p0, p1, p2, t);
                if (pt.x > p.x) {
                    // Direction based on endpoints
                    winding += (p2.y > p0.y) ? 1 : -1;
                }
            }
        }
    } else {
        float disc = b * b - 4.0 * a * c;
        if (disc >= 0.0) {
            float sqrtDisc = sqrt(disc);
            float t1 = (-b - sqrtDisc) / (2.0 * a);
            float t2 = (-b + sqrtDisc) / (2.0 * a);

            for (int i = 0; i < 2; i++) {
                float t = (i == 0) ? t1 : t2;
                if (t > 0.0 && t <= 1.0) {
                    vec2 pt = evalBezier(p0, p1, p2, t);
                    if (pt.x > p.x) {
                        // Use derivative sign at crossing point
                        float dy = 2.0 * a * t + b;
                        if (abs(dy) > 1e-7) {
                            winding += (dy > 0.0) ? 1 : -1;
                        }
                    }
                }
            }
        }
    }

    return winding;
}

// Distance to quadratic bezier curve (iterative approach)
float distanceToBezier(vec2 p, vec2 p0, vec2 p1, vec2 p2) {
    float bestT = 0.0;
    float bestDist = length(p - p0);

    const int SAMPLES = 16;
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
    for (int iter = 0; iter < 5; iter++) {
        float t = bestT;
        vec2 bp = evalBezier(p0, p1, p2, t);
        vec2 d1 = 2.0 * ((1.0 - t) * (p1 - p0) + t * (p2 - p1));
        vec2 d2 = 2.0 * (p2 - 2.0 * p1 + p0);

        vec2 diff = bp - p;
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

    vec2 glyphSize = glyph.bboxMax - glyph.bboxMin;

    // Account for padding in UV mapping
    float padding = fragStrokeThickness + 1.0;
    float scale = (fragSize.x - 2.0 * padding) / glyphSize.x;
    vec2 paddingInGlyphUnits = vec2(padding / scale);

    // Map UV to glyph space with padding offset (flip Y since font Y is up, screen Y is down)
    vec2 uv = vec2(fragUV.x, 1.0 - fragUV.y);
    vec2 paddedMin = glyph.bboxMin - paddingInGlyphUnits;
    vec2 paddedSize = glyphSize + 2.0 * paddingInGlyphUnits;
    vec2 p = paddedMin + uv * paddedSize;

    int winding = 0;
    float minDist = 1e10;

    for (uint c = 0; c < glyph.contourCount; c++) {
        Contour contour = getContour(glyph.contourStart + c);

        if (contour.pointCount < 3) continue;

        uint numBeziers = contour.pointCount / 2;
        for (uint i = 0; i < numBeziers; i++) {
            uint idx = contour.pointStart + i * 2;
            Point pt0 = getPoint(idx);
            Point pt1 = getPoint(idx + 1);
            Point pt2 = getPoint((i == numBeziers - 1) ? contour.pointStart : idx + 2);

            winding += bezierWindingContribution(p, pt0.pos, pt1.pos, pt2.pos);
            float d = distanceToBezier(p, pt0.pos, pt1.pos, pt2.pos);
            minDist = min(minDist, d);
        }
    }

    float pixelDist = minDist * scale;
    bool inside = (winding != 0);

    float signedDist = inside ? -pixelDist : pixelDist;

    // Fixed AA half-width in pixel space: 0.5 gives a 1-pixel transition band.
    // Using fwidth(pixelDist) is unstable at bezier segment boundaries.
    float aa = 0.5;

    // Subpixel AA: offset pixelDist (always positive, smooth) per RGB subpixel,
    // then reconstruct signed distance. Using dFdx(signedDist) directly is broken
    // at glyph edges where adjacent fragments in the 2x2 quad straddle the
    // inside/outside boundary, causing a sign flip and a huge derivative spike.
    float dpdx = dFdx(pixelDist);
    float subpixelOffset = 1.0 / 3.0;

    vec3 pixelDist3 = vec3(
        pixelDist - dpdx * subpixelOffset,
        pixelDist,
        pixelDist + dpdx * subpixelOffset
    );
    pixelDist3 = max(pixelDist3, vec3(0.0));

    float sign = inside ? -1.0 : 1.0;
    vec3 signedDist3 = sign * pixelDist3;

    vec3 fillAlpha3 = vec3(
        1.0 - smoothstep(-aa, aa, signedDist3.r),
        1.0 - smoothstep(-aa, aa, signedDist3.g),
        1.0 - smoothstep(-aa, aa, signedDist3.b)
    ) * fragColor.a;

    vec3 strokeAlpha3 = vec3(
        1.0 - smoothstep(fragStrokeThickness - aa, fragStrokeThickness + aa, pixelDist3.r),
        1.0 - smoothstep(fragStrokeThickness - aa, fragStrokeThickness + aa, pixelDist3.g),
        1.0 - smoothstep(fragStrokeThickness - aa, fragStrokeThickness + aa, pixelDist3.b)
    ) * fragStrokeColor.a;

    vec3 alpha3 = max(fillAlpha3, strokeAlpha3);

    if (max(max(alpha3.r, alpha3.g), alpha3.b) < 0.01) discard;

    // Per-channel color blend based on stroke vs fill contribution
    vec3 color = mix(fragColor.rgb, fragStrokeColor.rgb, strokeAlpha3 / max(alpha3, vec3(0.001)));

    // Dual-source blending: outColor carries premultiplied color,
    // outBlendFactor carries per-channel coverage for dst attenuation
    outColor = vec4(color * alpha3, max(max(alpha3.r, alpha3.g), alpha3.b));
    outBlendFactor = vec4(alpha3, max(max(alpha3.r, alpha3.g), alpha3.b));
}
