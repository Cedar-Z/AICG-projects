#version 410 core

// The geometry shader provides each clipped screen triangle to every candidate
// pixel. Flat inputs preserve its three vertices across the covering primitive.
flat in vec4 gScreenAndInvW[3];
flat in vec3 gWorld[3];
flat in vec3 gNormal[3];
flat in vec2 gUv[3];

layout(location = 0) out vec4 outColor;

// One directional light and the glTF metallic-roughness material of this draw item.
// Texture slots: base color, metallic-roughness, normal, occlusion, emissive.
uniform vec3 uCameraPosition;
uniform vec3 uLightDirection;
uniform vec3 uLightColor;
uniform float uLightIntensity;
uniform vec4 uBaseColorFactor;
uniform vec3 uEmissiveFactor;
uniform float uMetallicFactor;
uniform float uRoughnessFactor;
uniform float uOcclusionStrength;
uniform bool uHasTexture[5];
uniform sampler2D uTextures[5];
// Staged-debugging switch; the CPU rasterizer uses the same numbering in FrameState.
uniform int uDebugView;

// Color textures store sRGB values, while lighting calculations use linear RGB.
vec3 srgbToLinear(vec3 value) {
    vec3 result;
    for (int i = 0; i < 3; ++i) {
        result[i] = value[i] <= 0.04045 ? value[i] / 12.92 : pow((value[i] + 0.055) / 1.055, 2.4);
    }

    return result;
}

// Matches pbrShade() in pbr_shading.h. Explicit level-zero sampling avoids fragment
// derivatives changing filtering relative to the CPU backend.
vec3 pbrShade(vec3 worldPosition, vec3 normal, vec3 tangent, vec3 bitangent, vec2 uv) {
    const float kPi = 3.14159265358979323846;
    // Multiply the material factor by the base-color texel. Only color channels
    // need sRGB decoding; metallic, roughness, and normal textures are linear data.
    vec4 baseColor = uBaseColorFactor;
    if (uHasTexture[0]) {
        vec4 sampledColor = textureLod(uTextures[0], uv, 0.0);
        baseColor *= vec4(srgbToLinear(sampledColor.rgb), sampledColor.a);
    }
    float roughness = uRoughnessFactor;
    float metallic = uMetallicFactor;
    if (uHasTexture[1]) {
        // glTF packs roughness in G and metallic in B.
        vec4 sampledColor = textureLod(uTextures[1], uv, 0.0);
        roughness *= sampledColor.g;
        metallic *= sampledColor.b;
    }
    // Avoid a zero-width specular lobe and keep both parameters in valid ranges.
    roughness = clamp(roughness, 0.045, 1.0);
    metallic = clamp(metallic, 0.0, 1.0);

    // Interpolated vertex normals need renormalization. A normal-map texel is in
    // [0, 1]; remap it to [-1, 1] and transform through the world-space TBN basis.
    normal = normalize(normal);
    if (uHasTexture[2]) {
        vec3 tangentNormal = textureLod(uTextures[2], uv, 0.0).xyz * 2.0 - 1.0;
        // Remove the tangent's component along the normal before building TBN.
        vec3 orthogonalTangent = normalize(tangent - normal * dot(normal, tangent));
        normal = normalize(mat3(orthogonalTangent, normalize(bitangent), normal) * tangentNormal);
    }

    // V points toward the camera, L toward the light, and H bisects V and L.
    // The light transform stores its forward direction, so L uses its negative.
    vec3 viewDirection = normalize(uCameraPosition - worldPosition);
    vec3 lightDirection = normalize(-uLightDirection);
    vec3 halfway = normalize(viewDirection + lightDirection);
    float nDotL = max(dot(normal, lightDirection), 0.0);
    float nDotV = max(dot(normal, viewDirection), 0.0);
    float nDotH = max(dot(normal, halfway), 0.0);
    float vDotH = max(dot(viewDirection, halfway), 0.0);
    // GGX normal-distribution term D: roughness controls the highlight width.
    float alphaRoughness = roughness * roughness;
    float alphaSquared = alphaRoughness * alphaRoughness;
    float denominator = nDotH * nDotH * (alphaSquared - 1.0) + 1.0;
    float distribution = alphaSquared / max(kPi * denominator * denominator, 1e-5);

    // Smith geometry term G attenuates microfacets hidden from L or V.
    float attenuationL = 2.0 * nDotL / (nDotL + sqrt(alphaSquared + (1.0 - alphaSquared) * nDotL * nDotL));
    float attenuationV = 2.0 * nDotV / (nDotV + sqrt(alphaSquared + (1.0 - alphaSquared) * nDotV * nDotV));
    float geometry = attenuationL * attenuationV;

    // Schlick Fresnel F: dielectrics use F0 = 0.04, metals use their base color.
    // Combine D, G, and F for specular; only nonmetallic energy contributes to
    // Lambertian diffuse. nDotL applies the light's incidence angle.
    vec3 f0 = mix(vec3(0.04), baseColor.rgb, metallic);
    vec3 fresnel = f0 + (vec3(1.0) - f0) * pow(1.0 - vDotH, 5.0);
    vec3 specular = fresnel * distribution * geometry / max(4.0 * nDotL * nDotV, 1e-4);
    vec3 diffuse = (vec3(1.0) - fresnel) * (1.0 - metallic) * baseColor.rgb / kPi;
    vec3 color = (diffuse + specular) * uLightColor * uLightIntensity * nDotL;
    // This renderer applies the occlusion map to its single direct-light result.
    // Emissive is then added independently of the light direction and intensity.
    if (uHasTexture[3]) {
        float occlusion = textureLod(uTextures[3], uv, 0.0).r;
        color *= 1.0 + uOcclusionStrength * (occlusion - 1.0);
    }
    vec3 emissive = uEmissiveFactor;
    if (uHasTexture[4]) {
        emissive *= srgbToLinear(textureLod(uTextures[4], uv, 0.0).rgb);
    }

    return color + emissive;
}

void main() {
    vec2 p = gl_FragCoord.xy;
    vec2 a = gScreenAndInvW[0].xy;
    vec2 b = gScreenAndInvW[1].xy;
    vec2 c = gScreenAndInvW[2].xy;
    float area = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);

    // TODO: Determine whether the pixel center lies inside the screen triangle.
    // Evaluate the same three 2D edge functions as rasterizeTriangles() on the
    // CPU. gl_FragCoord.xy is the pixel center in framebuffer coordinates.
    // Hint: Use `discard` if the pixel is not inside the triangle
    /*************************************** TODO BEGIN ******************************************/
    // A nonpositive area means a back-facing or degenerate triangle. The CPU path
    // culls these, and face culling is not enabled for the candidate primitives.
    if (area <= 0.0) {
        discard;
    }

    // Edge functions normalized by the signed area: each weight is 1 at its own vertex.
    vec3 barycentric = vec3((b.x - p.x) * (c.y - p.y) - (b.y - p.y) * (c.x - p.x),
                            (c.x - p.x) * (a.y - p.y) - (c.y - p.y) * (a.x - p.x),
                            (a.x - p.x) * (b.y - p.y) - (a.y - p.y) * (b.x - p.x)) / area;

    // Negative weight means the sample is outside that edge; zero counts as covered.
    if (barycentric.x < 0.0 || barycentric.y < 0.0 || barycentric.z < 0.0) {
        discard;
    }
    /**************************************** TODO END *******************************************/

    // TODO: Write the pixel depth from screen-space barycentric coordinates.
    // Window-space depth is affine in the screen-space barycentrics.
    // Hint: write the depth value into gl_FragDepth
    /*************************************** TODO BEGIN ******************************************/
    // No 1/w correction here: window depth is already affine in screen space, exactly
    // like the depth test in rasterizeTriangles(). OpenGL performs the comparison.
    gl_FragDepth = barycentric.x * gScreenAndInvW[0].z +
                   barycentric.y * gScreenAndInvW[1].z +
                   barycentric.z * gScreenAndInvW[2].z;
    /**************************************** TODO END *******************************************/

    // TODO: Calculate the attributes needed to shade the pixel with pbrShade.
    // Hint: Use perspective-correct weights to interpolate world position, normal, and uv.
    // Hint: Derive tangent and bitangent from world-position and UV edges;
    //       keep the fallback axes when the UVs are degenerate.
    /*************************************** TODO BEGIN ******************************************/
    // Screen barycentrics interpolate 1/w linearly, so multiplying by each vertex's
    // invW and renormalizing yields the perspective-correct weights.
    vec3 invW = vec3(gScreenAndInvW[0].w, gScreenAndInvW[1].w, gScreenAndInvW[2].w);
    vec3 corrected = barycentric * invW;
    vec3 weights = corrected / (corrected.x + corrected.y + corrected.z);

    vec3 worldPosition = weights.x * gWorld[0] + weights.y * gWorld[1] + weights.z * gWorld[2];
    vec3 normal = weights.x * gNormal[0] + weights.y * gNormal[1] + weights.z * gNormal[2];
    vec2 uv = weights.x * gUv[0] + weights.y * gUv[1] + weights.z * gUv[2];

    // Tangent frame from world-space edges and UV deltas, as in buildScreenTriangles().
    // Degenerate UV mappings keep the fallback axes used by the CPU path.
    vec3 tangent = vec3(1.0, 0.0, 0.0);
    vec3 bitangent = vec3(0.0, 1.0, 0.0);
    vec3 edge1 = gWorld[1] - gWorld[0];
    vec3 edge2 = gWorld[2] - gWorld[0];
    vec2 delta1 = gUv[1] - gUv[0];
    vec2 delta2 = gUv[2] - gUv[0];
    float determinant = delta1.x * delta2.y - delta2.x * delta1.y;
    if (abs(determinant) > 1e-12) {
        float reciprocal = 1.0 / determinant;
        tangent = (edge1 * delta2.y - edge2 * delta1.y) * reciprocal;
        bitangent = (edge2 * delta1.x - edge1 * delta2.x) * reciprocal;
    }
    /**************************************** TODO END *******************************************/

    // Debug views write linear values of the same kind as pbrShade(), so both
    // backends go through the identical display pass and stay comparable.
    vec3 color;
    if (uDebugView == 1) {
        // Coverage only: the pixel is covered iff this constant color appears.
        color = vec3(0.8);
    } else if (uDebugView == 2) {
        // Screen-space barycentric weights, one channel per vertex.
        color = barycentric;
    } else if (uDebugView == 3) {
        // UV mapped directly to red and green.
        color = vec3(uv, 0.0);
    } else if (uDebugView == 4) {
        // Interpolated world normal remapped from [-1, 1] to [0, 1].
        color = normal * 0.5 + 0.5;
    } else {
        color = pbrShade(worldPosition, normal, tangent, bitangent, uv);
    }

    outColor = vec4(color, 1.0);
}
