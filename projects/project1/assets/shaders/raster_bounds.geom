#version 410 core

// Clip each source triangle like buildScreenTriangles() on the CPU, then emit
// candidate fragments for every triangle in the clipped polygon's fan.
layout(triangles) in;
layout(triangle_strip, max_vertices = 21) out;

in VertexData {
    vec4 clip;
    vec3 worldPosition;
    vec3 normal;
    vec2 uv;
} vertexData[];

// The fragment shader evaluates the screen-space edges itself. These values
// must stay constant across each generated candidate triangle.
flat out vec4 gScreenAndInvW[3];
flat out vec3 gWorld[3];
flat out vec3 gNormal[3];
flat out vec2 gUv[3];

uniform vec2 uExtent;

struct ClipVertex {
    vec4 clip;
    vec3 worldPosition;
    vec3 normal;
    vec2 uv;
};

struct ScreenVertex {
    // xy is pixel position, z is depth, and w is 1 / clip-space w.
    vec4 screenAndInvW;
    vec3 worldPosition;
    vec3 normal;
    vec2 uv;
};

// A triangle clipped by six planes has at most nine vertices. Keep the side
// where dot(plane, vertex.clip) >= 0, just as the CPU clipAgainst() does.
int clipAgainst(inout ClipVertex polygon[9], int count, vec4 plane) {
    // TODO: Clip the polygon against this plane with Sutherland-Hodgman.
    // Hint: visit every edge, including the edge from the last vertex to the first;
    // emit a crossing vertex when the endpoints are on opposite sides.
    /*************************************** TODO BEGIN ******************************************/
    // Scratch storage: a plane can add one vertex, so the result never exceeds nine.
    ClipVertex clipped[9];
    int clippedCount = 0;

    for (int current = 0; current < count; ++current) {
        ClipVertex from = polygon[current];
        ClipVertex to = polygon[(current + 1) % count];

        // A nonnegative signed distance keeps the vertex, exactly as on the CPU.
        float fromDistance = dot(plane, from.clip);
        float toDistance = dot(plane, to.clip);
        bool fromInside = fromDistance >= 0.0;
        bool toInside = toDistance >= 0.0;

        if (fromInside && clippedCount < 9) {
            clipped[clippedCount] = from;
            ++clippedCount;
        }

        // The endpoints are on opposite sides, so the denominator cannot vanish.
        if (fromInside != toInside && clippedCount < 9) {
            float t = fromDistance / (fromDistance - toDistance);
            clipped[clippedCount].clip = mix(from.clip, to.clip, t);
            clipped[clippedCount].worldPosition = mix(from.worldPosition, to.worldPosition, t);
            clipped[clippedCount].normal = mix(from.normal, to.normal, t);
            clipped[clippedCount].uv = mix(from.uv, to.uv, t);
            ++clippedCount;
        }
    }

    for (int current = 0; current < clippedCount; ++current) {
        polygon[current] = clipped[current];
    }

    return clippedCount;
    /**************************************** TODO END *******************************************/
}

void main() {
    ClipVertex polygon[9];
    int count = 3;
    for (int i = 0; i < 3; ++i) {
        polygon[i].clip = vertexData[i].clip;
        polygon[i].worldPosition = vertexData[i].worldPosition;
        polygon[i].normal = vertexData[i].normal;
        polygon[i].uv = vertexData[i].uv;
    }

    // OpenGL's homogeneous clip volume is -w <= x, y, z <= w.

    // Clip the source triangle against all six homogeneous clip planes.
    count = clipAgainst(polygon, count, vec4(1, 0, 0, 1));  // left: x + w >= 0
    count = clipAgainst(polygon, count, vec4(-1, 0, 0, 1)); // right: w - x >= 0
    count = clipAgainst(polygon, count, vec4(0, 1, 0, 1));  // bottom: y + w >= 0
    count = clipAgainst(polygon, count, vec4(0, -1, 0, 1)); // top: w - y >= 0
    count = clipAgainst(polygon, count, vec4(0, 0, 1, 1));  // near: z + w >= 0
    count = clipAgainst(polygon, count, vec4(0, 0, -1, 1)); // far: w - z >= 0

    // A convex clipped polygon becomes (0, 1, 2), (0, 2, 3), ... .
    for (int fan = 1; fan + 1 < count; ++fan) {
        ClipVertex triangle[3] = ClipVertex[3](polygon[0], polygon[fan], polygon[fan + 1]);
        ScreenVertex screenTriangle[3];
        // TODO: Convert each clip-space vertex to a ScreenVertex.
        // Hint: divide xyz by clip.w, map NDC xy to pixels and z to [0, 1],
        // and retain 1 / clip.w for perspective-correct interpolation.
        /*************************************** TODO BEGIN ******************************************/
        for (int corner = 0; corner < 3; ++corner) {
            float invW = 1.0 / triangle[corner].clip.w;
            vec3 ndc = triangle[corner].clip.xyz * invW;

            // Same viewport mapping as buildScreenTriangles(): origin bottom-left, Y up.
            screenTriangle[corner].screenAndInvW =
                vec4((ndc.xy * 0.5 + 0.5) * uExtent, ndc.z * 0.5 + 0.5, invW);
            screenTriangle[corner].worldPosition = triangle[corner].worldPosition;
            screenTriangle[corner].normal = triangle[corner].normal;
            screenTriangle[corner].uv = triangle[corner].uv;
        }
        /**************************************** TODO END *******************************************/

        // TODO: Bound the candidate pixels and emit a primitive covering them.
        // Hint: Convert the screen-space bounds back to NDC; the fragment shader
        //       will perform the exact per-pixel triangle coverage test.
        // Hint: Use EmitVertex/EmitPrimitive
        /*************************************** TODO BEGIN ******************************************/
        // One pixel of padding, so samples lying on a rounded bound are still covered.
        vec2 lower = min(min(screenTriangle[0].screenAndInvW.xy, screenTriangle[1].screenAndInvW.xy),
                         screenTriangle[2].screenAndInvW.xy) - vec2(1.0);
        vec2 upper = max(max(screenTriangle[0].screenAndInvW.xy, screenTriangle[1].screenAndInvW.xy),
                         screenTriangle[2].screenAndInvW.xy) + vec2(1.0);
        vec2 span = max(upper - lower, vec2(2.0));

        // A right triangle whose hypotenuse touches the padded far corner of the box,
        // so the whole bounding rectangle is inside one primitive (three vertices only).
        vec2 corners[3] = vec2[3](lower,
                                  lower + vec2(2.0 * span.x, 0.0),
                                  lower + vec2(0.0, 2.0 * span.y));

        // Constant across the emitted primitive: the fragment stage re-evaluates the
        // original screen triangle from these three vertices.
        gScreenAndInvW[0] = screenTriangle[0].screenAndInvW;
        gScreenAndInvW[1] = screenTriangle[1].screenAndInvW;
        gScreenAndInvW[2] = screenTriangle[2].screenAndInvW;
        gWorld[0] = screenTriangle[0].worldPosition;
        gWorld[1] = screenTriangle[1].worldPosition;
        gWorld[2] = screenTriangle[2].worldPosition;
        gNormal[0] = screenTriangle[0].normal;
        gNormal[1] = screenTriangle[1].normal;
        gNormal[2] = screenTriangle[2].normal;
        gUv[0] = screenTriangle[0].uv;
        gUv[1] = screenTriangle[1].uv;
        gUv[2] = screenTriangle[2].uv;

        for (int corner = 0; corner < 3; ++corner) {
            // Pixels back to NDC. Depth comes from gl_FragDepth, so z is irrelevant
            // here, and the primitive may extend past the viewport (OpenGL clips it).
            gl_Position = vec4(corners[corner] / uExtent * 2.0 - 1.0, 0.0, 1.0);
            EmitVertex();
        }
        EndPrimitive();
        /**************************************** TODO END *******************************************/
    }
}
