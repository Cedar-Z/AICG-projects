#pragma once

#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "raster_types.h"

// Procedural scenes used to check individual pipeline stages (README 8.2).
//
// Every item is authored directly in world space and rendered with an identity
// model matrix, so the tests stay still while screenshots are taken. The camera
// is the application default: eye (0, 0, 3.6), fov 45 degrees, near 0.1, far 100,
// which puts the visible area at z = 0 at roughly x in [-2.65, 2.65] and
// y in [-1.49, 1.49].
class DebugScene {
public:
    enum class Page { Stage, Clip };

    // Requires a current OpenGL context: the checker texture is uploaded for both
    // rasterizers here.
    DebugScene() {
        createTextures();
        createStagePage();
        createClipPage();
    }

    std::vector<DrawItem> const& items(Page page) const {
        return page == Page::Stage ? m_stageItems : m_clipItems;
    }

private:
    // ---------------------------------------------------------------- helpers

    static Vertex makeVertex(glm::vec3 position, glm::vec2 uv, glm::vec3 normal = glm::vec3(0.0f, 0.0f, 1.0f)) {
        Vertex vertex;
        vertex.position = position;
        vertex.normal = normal;
        vertex.uv = uv;
        return vertex;
    }

    Mesh* addMesh(std::vector<Vertex> const& vertices, std::vector<uint32_t> const& indices) {
        auto mesh = std::make_unique<Mesh>();
        mesh->editVertices() = vertices;
        mesh->editIndices() = indices;
        m_meshes.push_back(std::move(mesh));
        return m_meshes.back().get();
    }

    // Axis-aligned quad in the z = constant plane, counter-clockwise when seen from +Z.
    Mesh* addQuad(glm::vec2 center, glm::vec2 halfExtent, float z, float uvRepeat) {
        std::vector<Vertex> vertices{
            makeVertex({center.x - halfExtent.x, center.y - halfExtent.y, z}, {0.0f, 0.0f}),
            makeVertex({center.x + halfExtent.x, center.y - halfExtent.y, z}, {uvRepeat, 0.0f}),
            makeVertex({center.x + halfExtent.x, center.y + halfExtent.y, z}, {uvRepeat, uvRepeat}),
            makeVertex({center.x - halfExtent.x, center.y + halfExtent.y, z}, {0.0f, uvRepeat}),
        };
        return addMesh(vertices, {0, 1, 2, 0, 2, 3});
    }

    // Counter-clockwise triangle; pass counterClockwise = false to author the same
    // geometry with reversed winding, which must be culled as a back face.
    Mesh* addTriangle(glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec2 uvA, glm::vec2 uvB, glm::vec2 uvC,
                      bool counterClockwise = true) {
        std::vector<Vertex> vertices{makeVertex(a, uvA), makeVertex(b, uvB), makeVertex(c, uvC)};
        if (counterClockwise) {
            return addMesh(vertices, {0, 1, 2});
        }
        return addMesh(vertices, {0, 2, 1});
    }

    PbrMaterial* addFlatMaterial(glm::vec3 const& baseColor) {
        auto material = std::make_unique<PbrMaterial>();
        material->baseColorFactor = glm::vec4(baseColor, 1.0f);
        material->metallicFactor = 0.0f;
        material->roughnessFactor = 0.55f;
        m_materials.push_back(std::move(material));
        return m_materials.back().get();
    }

    PbrMaterial* addTexturedMaterial(UnifiedTexture2D const& texture) {
        auto material = addFlatMaterial(glm::vec3(1.0f));
        material->hasTexture[PbrMaterial::BaseColor] = true;
        material->textures[PbrMaterial::BaseColor] = texture;
        return material;
    }

    PbrMaterial* addCheckerMaterial() { return addTexturedMaterial(m_checker); }

    void addItem(std::vector<DrawItem>& items, Mesh const* mesh, PbrMaterial const* material,
                 glm::mat4 const& meshToScene = glm::mat4(1.0f)) {
        items.push_back(DrawItem{.mesh = mesh, .material = material, .meshToScene = meshToScene});
    }

    // ---------------------------------------------------------------- texture

    static std::vector<unsigned char> makeCheckerPixels(int size, int cells) {
        std::vector<unsigned char> pixels(static_cast<size_t>(size) * size * 4, 255);
        int const cellSize = size / cells;
        for (int y = 0; y < size; ++y) {
            for (int x = 0; x < size; ++x) {
                int const cell = (x / cellSize + y / cellSize) % 2;
                unsigned char const value = cell == 0 ? 235 : 70;
                size_t const offset = (static_cast<size_t>(y) * size + x) * 4;
                pixels[offset + 0] = value;
                pixels[offset + 1] = value;
                pixels[offset + 2] = value;
                pixels[offset + 3] = 255;
                // Red square in the corner marks the (u, v) = (0, 0) texel so a flipped
                // or transposed UV mapping is visible at a glance.
                if (x < cellSize && y < cellSize) {
                    pixels[offset + 0] = 230;
                    pixels[offset + 1] = 40;
                    pixels[offset + 2] = 40;
                }
            }
        }
        return pixels;
    }

    static UnifiedTexture2D makeTexture(int size, std::vector<unsigned char> const& pixels) {
        UnifiedTexture2D texture;
        texture.cpuTexture = std::make_shared<CpuTexture2D>(CpuTexture2D::InternalFormat::RGBA8,
                                                            size,
                                                            size,
                                                            CpuTexture2D::Format::RedGreenBlueAlpha,
                                                            CpuTexture2D::DataType::UnsignedByte,
                                                            pixels.data());
        texture.cpuSampler = std::make_shared<CpuSampler>(CpuSampler::Description{
            .minFilter = CpuSampler::Filter::Linear,
            .magFilter = CpuSampler::Filter::Linear,
            .addressModeU = CpuSampler::AddressMode::Repeat,
            .addressModeV = CpuSampler::AddressMode::Repeat,
        });
        texture.gpuTexture = std::make_shared<gfx::Texture2D>(
            gfx::Texture2D::Specification{
                .width = static_cast<uint32_t>(size),
                .height = static_cast<uint32_t>(size),
                .format = gfx::Texture::Format::RGBA8,
                .pixelFormat = gfx::Texture::ExternalFormat::RGBA,
                .pixelType = gfx::Texture::PixelType::UByte,
                .generateMipmap = false,
                .mipmapLevels = 1,
                .isImmutable = false,
            },
            pixels.data());
        texture.gpuSampler = std::make_shared<gfx::Sampler>(gfx::Sampler::Description{
            .minFilter = gfx::Sampler::Filter::Linear,
            .magFilter = gfx::Sampler::Filter::Linear,
            .addressModeU = gfx::Sampler::AddressMode::Repeat,
            .addressModeV = gfx::Sampler::AddressMode::Repeat,
        });
        return texture;
    }

    void createTextures() {
        constexpr int kSize = 256;
        m_checker = makeTexture(kSize, makeCheckerPixels(kSize, 8));
    }

    // ------------------------------------------------------------------ pages

    // Ten cells in two rows, placed below the UI panel so no test is covered by ImGui:
    //   row 1 (y = +0.05): front triangle, reversed winding, collinear, behind camera,
    //                      depth order A (near drawn first)
    //   row 2 (y = -0.95): depth order B (far drawn first), shared edge, tilted
    //                      checkerboard, degenerate UV, non-uniform scale
    void createStagePage() {
        PbrMaterial* checker = addCheckerMaterial();
        PbrMaterial* red = addFlatMaterial(glm::vec3(0.35f, 0.05f, 0.05f));
        PbrMaterial* green = addFlatMaterial(glm::vec3(0.05f, 0.30f, 0.08f));

        constexpr float kHalfWidth = 0.40f;
        constexpr float kHalfHeight = 0.34f;
        float const rowTop = 0.05f;
        float const rowBottom = -0.95f;
        float const column[5] = {-2.10f, -1.05f, 0.00f, 1.05f, 2.10f};

        // 1. Front-facing textured triangle: interior must be continuous and edges sharp.
        addItem(m_stageItems,
                addTriangle({column[0] - kHalfWidth, rowTop - kHalfHeight, 0.0f},
                            {column[0] + kHalfWidth, rowTop - kHalfHeight, 0.0f},
                            {column[0], rowTop + kHalfHeight, 0.0f},
                            {0.0f, 0.0f},
                            {1.5f, 0.0f},
                            {0.75f, 1.5f}),
                checker);

        // 2. Same geometry with reversed winding: must be culled, leaving an empty cell.
        addItem(m_stageItems,
                addTriangle({column[1] - kHalfWidth, rowTop - kHalfHeight, 0.0f},
                            {column[1] + kHalfWidth, rowTop - kHalfHeight, 0.0f},
                            {column[1], rowTop + kHalfHeight, 0.0f},
                            {0.0f, 0.0f},
                            {1.5f, 0.0f},
                            {0.75f, 1.5f},
                            false),
                checker);

        // 3. Three collinear points: no area, so no fragment and no division by zero.
        addItem(m_stageItems,
                addTriangle({column[2] - kHalfWidth, rowTop, 0.0f},
                            {column[2], rowTop, 0.0f},
                            {column[2] + kHalfWidth, rowTop, 0.0f},
                            {0.0f, 0.0f},
                            {0.5f, 0.5f},
                            {1.0f, 1.0f}),
                checker);

        // 4. Inside the frustum in x/y but behind the camera: the near plane clips it away.
        addItem(m_stageItems, addQuad({column[3], rowTop}, {kHalfWidth, kHalfHeight}, 5.0f, 1.0f), checker);

        // 5. Two triangles at different depths, near one drawn first. The far (red) one
        //    must stay hidden even though it is issued last.
        addItem(m_stageItems,
                addTriangle({column[4] - kHalfWidth * 0.6f, rowTop - kHalfHeight, 0.3f},
                            {column[4] + kHalfWidth * 0.6f, rowTop - kHalfHeight, 0.3f},
                            {column[4], rowTop + kHalfHeight, 0.3f},
                            {0.0f, 1.0f},
                            {1.0f, 1.0f},
                            {0.5f, 0.0f}),
                green);
        addItem(m_stageItems,
                addTriangle({column[4] - kHalfWidth, rowTop - kHalfHeight * 0.5f, -0.4f},
                            {column[4] + kHalfWidth, rowTop - kHalfHeight * 0.5f, -0.4f},
                            {column[4], rowTop + kHalfHeight * 0.5f, -0.4f},
                            {0.0f, 0.0f},
                            {2.0f, 0.0f},
                            {1.0f, 2.0f}),
                red);

        // 6. The same pair in the opposite issue order: the image must be identical,
        //    which is what "depth test instead of painter's algorithm" buys.
        addItem(m_stageItems,
                addTriangle({column[0] - kHalfWidth, rowBottom - kHalfHeight * 0.5f, -0.4f},
                            {column[0] + kHalfWidth, rowBottom - kHalfHeight * 0.5f, -0.4f},
                            {column[0], rowBottom + kHalfHeight * 0.5f, -0.4f},
                            {0.0f, 0.0f},
                            {2.0f, 0.0f},
                            {1.0f, 2.0f}),
                red);
        addItem(m_stageItems,
                addTriangle({column[0] - kHalfWidth * 0.6f, rowBottom - kHalfHeight, 0.3f},
                            {column[0] + kHalfWidth * 0.6f, rowBottom - kHalfHeight, 0.3f},
                            {column[0], rowBottom + kHalfHeight, 0.3f},
                            {0.0f, 1.0f},
                            {1.0f, 1.0f},
                            {0.5f, 0.0f}),
                green);

        // 7. A square split along its diagonal into two triangles that share the edge
        //    (x0, y0)-(x1, y1): no crack, and no double blend on the seam.
        {
            float const x0 = column[1] - kHalfWidth;
            float const x1 = column[1] + kHalfWidth;
            float const y0 = rowBottom - kHalfHeight;
            float const y1 = rowBottom + kHalfHeight;
            Mesh* lowerRight = addTriangle({x0, y0, 0.0f}, {x1, y0, 0.0f}, {x1, y1, 0.0f}, {0.0f, 0.0f}, {2.0f, 0.0f}, {2.0f, 2.0f});
            Mesh* upperLeft = addTriangle({x0, y0, 0.0f}, {x1, y1, 0.0f}, {x0, y1, 0.0f}, {0.0f, 0.0f}, {2.0f, 2.0f}, {0.0f, 2.0f});
            addItem(m_stageItems, lowerRight, checker);
            addItem(m_stageItems, upperLeft, checker);
        }

        // 8. Tilted checkerboard: perspective must compress the squares with distance.
        //    The rotation is negative so the normal still faces the light; otherwise the
        //    quad would land in shadow and the texture test would say nothing.
        {
            Mesh* quad = addQuad({0.0f, 0.0f}, {kHalfWidth, kHalfHeight}, 0.0f, 3.0f);
            glm::mat4 transform = glm::translate(glm::mat4(1.0f), glm::vec3(column[2], rowBottom, 0.0f));
            transform = glm::rotate(transform, glm::radians(-35.0f), glm::vec3(0.0f, 1.0f, 0.0f));
            transform = glm::rotate(transform, glm::radians(12.0f), glm::vec3(1.0f, 0.0f, 0.0f));
            addItem(m_stageItems, quad, checker, transform);
        }

        // 9. Degenerate UVs (all three vertices sample the same texel): the tangent frame
        //    falls back to the default axes and the surface must shade as one flat color.
        addItem(m_stageItems,
                addTriangle({column[3] - kHalfWidth, rowBottom - kHalfHeight, 0.0f},
                            {column[3] + kHalfWidth, rowBottom - kHalfHeight, 0.0f},
                            {column[3], rowBottom + kHalfHeight, 0.0f},
                            {0.5f, 0.5f},
                            {0.5f, 0.5f},
                            {0.5f, 0.5f}),
                checker);

        // 10. Non-uniform scale: the inverse-transpose normal transform must keep the
        //     flat face evenly lit instead of skewing the shading toward the stretched axis.
        {
            Mesh* quad = addQuad({0.0f, 0.0f}, {kHalfWidth, kHalfHeight}, 0.0f, 2.0f);
            glm::mat4 transform = glm::translate(glm::mat4(1.0f), glm::vec3(column[4], rowBottom, 0.0f));
            transform = glm::scale(transform, glm::vec3(1.3f, 0.45f, 1.0f));
            addItem(m_stageItems, quad, checker, transform);
        }
    }

    // Near-plane clipping is meant to be invisible: the cut edge lands outside the
    // viewport, so "the screen is still full of colour" says nothing. What the test can
    // show is that the visible half is a consistent perspective image of an asymmetric
    // texture (every tile carries a red block in its (u,v) = (0,0) corner), that the area
    // behind the camera was dropped instead of being wrapped into a mirrored copy, and
    // that opaque markers in front still win the depth test against the clipped geometry.
    void createClipPage() {
        PbrMaterial* checker = addCheckerMaterial();

        // Two vertices in front of the near plane, one well behind the camera (z = +4.6,
        // i.e. view-space depth +1.0 and w < 0). The texture repeats three times so the
        // magnified on-screen result still reads as a grid: a mirrored copy would put the
        // red corner blocks on the wrong side of every tile, whereas a once-only mapping
        // collapses into three giant blocks.
        addItem(m_clipItems,
                addTriangle({-3.6f, -1.35f, -0.5f},
                            {3.6f, -1.35f, -0.5f},
                            {0.0f, 2.6f, 4.6f},
                            {0.0f, 0.0f},
                            {3.0f, 0.0f},
                            {1.5f, 3.0f}),
                checker);

        // Three opaque markers at different depths, all nearer to the camera than the
        // clipped triangle. They must be fully visible and correctly occlude it.
        // The triangle plane sits at z = +0.21 where these markers are, i.e. depth 3.39,
        // so the farthest marker has to stay clearly in front of that (z = 0.50), otherwise
        // the depth test correctly hides it and the test proves nothing.
        // Their x offsets shrink with depth, otherwise the shallow ones land outside the
        // narrowing frustum (the half-width at w = 2.4 is only 0.725 in NDC).
        PbrMaterial* white = addFlatMaterial(glm::vec3(0.95f, 0.95f, 0.95f));
        PbrMaterial* yellow = addFlatMaterial(glm::vec3(0.95f, 0.80f, 0.10f));
        PbrMaterial* cyan = addFlatMaterial(glm::vec3(0.10f, 0.80f, 0.90f));
        addItem(m_clipItems, addQuad({-1.20f, -0.80f}, {0.20f, 0.13f}, 1.20f, 1.0f), white);
        addItem(m_clipItems, addQuad({0.00f, -0.80f}, {0.20f, 0.13f}, 0.85f, 1.0f), yellow);
        addItem(m_clipItems, addQuad({1.20f, -0.80f}, {0.20f, 0.13f}, 0.50f, 1.0f), cyan);
    }

private:
    std::vector<std::unique_ptr<Mesh>> m_meshes;
    std::vector<std::unique_ptr<PbrMaterial>> m_materials;
    std::vector<DrawItem> m_stageItems;
    std::vector<DrawItem> m_clipItems;
    UnifiedTexture2D m_checker;
};
