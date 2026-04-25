#include "ParticleSystem.h"
#include "Shader.h"

#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/constants.hpp>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace Marble {

// ── Particle struct ────────────────────────────────────────────────────────────
// File-local — the public header exposes only the EmitParams API.
// Layout is designed to match the GLSL std430 Particle struct byte-for-byte.
// Must NOT be reordered without updating both this struct and the GLSL sources.
struct Particle {
  glm::vec2 pos;        //  8 bytes — offset  0
  glm::vec2 vel;        //  8 bytes — offset  8
  glm::vec4 colorStart; // 16 bytes — offset 16
  glm::vec4 colorEnd;   // 16 bytes — offset 32
  float     life;       //  4 bytes — offset 48
  float     maxLife;    //  4 bytes — offset 52
  float     sizeStart;  //  4 bytes — offset 56
  float     sizeEnd;    //  4 bytes — offset 60
  float     drag;       //  4 bytes — offset 64
  float     _pad[3];    // 12 bytes — offset 68  (pads struct to 80, a multiple of 16)
};

// ── Layout verification ────────────────────────────────────────────────────────
// These asserts guarantee the C++ struct matches the GLSL std430 layout exactly.
// If any assert fires the GPU will read garbage — fix the struct, not the assert.
static_assert(sizeof(Particle)             == 80, "Particle size mismatch");
static_assert(offsetof(Particle, pos)        ==  0, "pos offset mismatch");
static_assert(offsetof(Particle, vel)        ==  8, "vel offset mismatch");
static_assert(offsetof(Particle, colorStart) == 16, "colorStart offset mismatch");
static_assert(offsetof(Particle, colorEnd)   == 32, "colorEnd offset mismatch");
static_assert(offsetof(Particle, life)       == 48, "life offset mismatch");
static_assert(offsetof(Particle, maxLife)    == 52, "maxLife offset mismatch");
static_assert(offsetof(Particle, sizeStart)  == 56, "sizeStart offset mismatch");
static_assert(offsetof(Particle, sizeEnd)    == 60, "sizeEnd offset mismatch");
static_assert(offsetof(Particle, drag)       == 64, "drag offset mismatch");

// ── Compute shader ─────────────────────────────────────────────────────────────
// 256 threads per work group — saturates a warp/wavefront on both NVIDIA and AMD.
// Each invocation advances one particle by u_DeltaTime seconds.
// Dead particles (life <= 0) are left as-is; the ring buffer overwrites them on
// the next Emit rather than zeroing them here (avoids an unnecessary write).
static constexpr const char* k_ComputeSrc = R"GLSL(
#version 460 core
layout(local_size_x = 256) in;

struct Particle {
    vec2  pos;
    vec2  vel;
    vec4  colorStart;
    vec4  colorEnd;
    float life;
    float maxLife;
    float sizeStart;
    float sizeEnd;
    float drag;
    float _pad[3];
};

layout(std430, binding = 0) buffer ParticleBuffer {
    Particle particles[];
};

uniform float u_DeltaTime;
uniform uint  u_Count;

void main() {
    uint idx = gl_GlobalInvocationID.x;
    if (idx >= u_Count) return;

    Particle p = particles[idx];
    if (p.life <= 0.0) return;

    p.life -= u_DeltaTime;
    if (p.life <= 0.0) {
        p.life = 0.0;
        particles[idx].life = 0.0;
        return;
    }

    // Exponential velocity decay: v(t) = v0 * e^(-drag * dt).
    // More physically correct than a flat multiplier and frame-rate independent.
    float dragFactor = exp(-p.drag * u_DeltaTime);
    p.vel *= dragFactor;

    p.pos += p.vel * u_DeltaTime;

    particles[idx] = p;
}
)GLSL";

// ── Render shaders ─────────────────────────────────────────────────────────────
// The vertex shader generates a two-triangle quad (6 vertices) per particle
// by reading gl_VertexID without any bound vertex attributes.
// Dead particles (life <= 0) output degenerate zero-area triangles that the
// rasteriser discards without producing any fragments.
static constexpr const char* k_VertSrc = R"GLSL(
#version 460 core

struct Particle {
    vec2  pos;
    vec2  vel;
    vec4  colorStart;
    vec4  colorEnd;
    float life;
    float maxLife;
    float sizeStart;
    float sizeEnd;
    float drag;
    float _pad[3];
};

layout(std430, binding = 0) readonly buffer ParticleBuffer {
    Particle particles[];
};

uniform mat4 u_ViewProjection;

out vec4 v_Color;
out vec2 v_UV;   // [-0.5, 0.5] over the quad face

// Two-triangle quad, CCW winding.
const vec2 k_Corners[6] = vec2[6](
    vec2(-0.5, -0.5), vec2( 0.5, -0.5), vec2( 0.5,  0.5),
    vec2(-0.5, -0.5), vec2( 0.5,  0.5), vec2(-0.5,  0.5)
);

void main() {
    int pIdx = gl_VertexID / 6;
    int vIdx = gl_VertexID % 6;

    Particle p = particles[pIdx];

    // Dead particle — degenerate triangle, zero area, no fragments produced.
    if (p.life <= 0.0) {
        gl_Position = vec4(0.0, 0.0, 0.0, 1.0);
        v_Color     = vec4(0.0);
        v_UV        = vec2(0.0);
        return;
    }

    // Normalised age in [0, 1] where 0 = just born, 1 = about to die.
    float t     = 1.0 - (p.life / p.maxLife);
    float size  = mix(p.sizeStart, p.sizeEnd, t);
    v_Color     = mix(p.colorStart, p.colorEnd, t);

    vec2 corner  = k_Corners[vIdx];
    v_UV         = corner;
    vec2 worldPos = p.pos + corner * size;
    gl_Position   = u_ViewProjection * vec4(worldPos, 0.0, 1.0);
}
)GLSL";

// Soft circular disc with smooth edge falloff.
// Reads v_UV in [-0.5, 0.5] — distance from center is 0.5 at the quad edge.
static constexpr const char* k_FragSrc = R"GLSL(
#version 460 core

in vec4 v_Color;
in vec2 v_UV;

out vec4 FragColor;

void main() {
    // Map UV distance to [0, 1]: 0 at center, 1 at the corner of the unit quad.
    // The circular edge is at distance 0.5 in UV space, which maps to dist = 1.0.
    float dist  = length(v_UV) * 2.0;

    // Bright core (dist < 0.4) fades sharply into transparent edge (dist > 1.0).
    // Two-stage profile: crisp bright center + soft outer halo.
    float core  = 1.0 - smoothstep(0.0, 0.4, dist);
    float halo  = 1.0 - smoothstep(0.4, 1.0, dist);
    float alpha = mix(halo, 1.0, core * 0.6);

    FragColor = vec4(v_Color.rgb, v_Color.a * alpha);
}
)GLSL";

// ── Constructor / Destructor ───────────────────────────────────────────────────

ParticleSystem::ParticleSystem(int maxParticles)
    : m_MaxParticles(maxParticles)
{
    assert(maxParticles > 0 && maxParticles % 256 == 0 &&
           "maxParticles must be positive and a multiple of 256");

    // ── Compile shaders ───────────────────────────────────────────────────────
    m_ComputeShader = std::make_unique<Shader>(Shader::Compute, k_ComputeSrc);
    m_RenderShader  = std::make_unique<Shader>(Shader::FromSource, k_VertSrc, k_FragSrc);

    // ── Allocate SSBO ─────────────────────────────────────────────────────────
    // Zero-initialise so the GPU sees life = 0 for all particles from the start.
    const GLsizeiptr bufferBytes =
        static_cast<GLsizeiptr>(m_MaxParticles) * static_cast<GLsizeiptr>(sizeof(Particle));

    glGenBuffers(1, &m_GL.ssbo);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_GL.ssbo);
    glBufferData(GL_SHADER_STORAGE_BUFFER, bufferBytes, nullptr, GL_DYNAMIC_DRAW);
    // Zero out so GPU sees life=0 (dead) for all slots initially.
    {
        void* ptr = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, bufferBytes,
                                     GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
        if (ptr) std::memset(ptr, 0, static_cast<size_t>(bufferBytes));
        glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
    }
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    // ── Empty VAO ─────────────────────────────────────────────────────────────
    // OpenGL Core Profile requires a VAO bound for draw calls even when no
    // vertex attributes are used (we read everything from the SSBO in the VS).
    glGenVertexArrays(1, &m_GL.vao);
}

ParticleSystem::~ParticleSystem() {
    glDeleteBuffers(1, &m_GL.ssbo);
    glDeleteVertexArrays(1, &m_GL.vao);
}

// ── Emit ──────────────────────────────────────────────────────────────────────

void ParticleSystem::Emit(const EmitParams& params, std::mt19937& rng) {
    if (params.count <= 0) return;

    const float baseAngle = std::atan2(params.velocity.y, params.velocity.x);
    const float baseSpeed = glm::length(params.velocity);

    std::uniform_real_distribution<float> angleDist(
        -params.velocitySpreadAngle, params.velocitySpreadAngle);
    std::uniform_real_distribution<float> speedDist(
        -params.speedSpread, params.speedSpread);
    std::uniform_real_distribution<float> lifeDist(
        params.lifetimeMin, params.lifetimeMax);

    // Build all particles in a CPU-side staging buffer first so we can upload
    // to the SSBO in at most two glBufferSubData calls (one per wrap edge).
    const int count = std::min(params.count, m_MaxParticles);
    std::vector<Particle> staging(static_cast<size_t>(count));

    for (int i = 0; i < count; ++i) {
        const float angle    = baseAngle + angleDist(rng);
        const float speed    = std::max(0.0f, baseSpeed + speedDist(rng));
        const float lifetime = lifeDist(rng);

        Particle& p  = staging[static_cast<size_t>(i)];
        p.pos        = params.position;
        p.vel        = { speed * std::cos(angle), speed * std::sin(angle) };
        p.colorStart = params.colorStart;
        p.colorEnd   = params.colorEnd;
        p.life       = lifetime;
        p.maxLife    = lifetime;
        p.sizeStart  = params.sizeStart;
        p.sizeEnd    = params.sizeEnd;
        p.drag       = params.drag;
        std::memset(p._pad, 0, sizeof(p._pad));
    }

    // Upload to the ring buffer — at most two contiguous uploads handle the wrap.
    const int    stride     = static_cast<int>(sizeof(Particle));
    const int    firstSlot  = m_NextSlot;
    const int    endSlot    = (m_NextSlot + count) % m_MaxParticles; // exclusive
    m_NextSlot = endSlot;

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_GL.ssbo);

    if (firstSlot + count <= m_MaxParticles) {
        // Contiguous range — single upload.
        glBufferSubData(GL_SHADER_STORAGE_BUFFER,
                        static_cast<GLintptr>(firstSlot) * stride,
                        static_cast<GLsizeiptr>(count)   * stride,
                        staging.data());
    } else {
        // Wraps around end of buffer — two uploads.
        const int firstChunk = m_MaxParticles - firstSlot;
        const int secondChunk = count - firstChunk;

        glBufferSubData(GL_SHADER_STORAGE_BUFFER,
                        static_cast<GLintptr>(firstSlot) * stride,
                        static_cast<GLsizeiptr>(firstChunk) * stride,
                        staging.data());
        glBufferSubData(GL_SHADER_STORAGE_BUFFER,
                        0,
                        static_cast<GLsizeiptr>(secondChunk) * stride,
                        staging.data() + firstChunk);
    }

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

// ── Update ────────────────────────────────────────────────────────────────────

void ParticleSystem::Update(float dt) {
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_GL.ssbo);

    m_ComputeShader->Bind();
    m_ComputeShader->SetFloat("u_DeltaTime", dt);
    // u_Count is uint in the shader — pass as int, GLSL handles the reinterpret.
    m_ComputeShader->SetInt("u_Count", m_MaxParticles);

    // One work group per 256 particles — covers the full buffer.
    const uint32_t groups = static_cast<uint32_t>(m_MaxParticles) / 256u;
    m_ComputeShader->Dispatch(groups);

    // Ensure the compute writes are visible to the vertex shader SSBO reads
    // before the render pass begins. GL_SHADER_STORAGE_BARRIER_BIT covers both.
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, 0);
    m_ComputeShader->Unbind();
}

// ── Render ────────────────────────────────────────────────────────────────────

void ParticleSystem::Render(const OrthographicCamera& camera) {
    // Additive blending: each particle adds its light to what is behind it.
    // This makes overlapping particles glow rather than overdraw opaquely.
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_GL.ssbo);

    m_RenderShader->Bind();
    m_RenderShader->SetMat4("u_ViewProjection", camera.GetViewProjection());

    glBindVertexArray(m_GL.vao);
    // 6 vertices per particle (two triangles); dead ones produce zero-area
    // degenerate triangles that the rasteriser discards at no fragment cost.
    glDrawArrays(GL_TRIANGLES, 0, m_MaxParticles * 6);
    glBindVertexArray(0);

    m_RenderShader->Unbind();
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, 0);

    // Restore standard alpha blending used by the rest of the engine.
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

} // namespace Marble
