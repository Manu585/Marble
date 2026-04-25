#pragma once
#include "Camera.h"
#include <glm/glm.hpp>
#include <cstdint>
#include <memory>
#include <random>
#include <vector>

namespace Marble {

  // GPU-simulated particle system backed by a Shader Storage Buffer Object (SSBO).
  //
  // Simulation runs entirely on the GPU via a compute shader dispatched each frame.
  // Emission writes particle data directly into the SSBO from the CPU using
  // glBufferSubData, using a ring buffer to reclaim slots without GPU readback.
  //
  // Rendering uses a geometry-less draw: the vertex shader reads particle data from
  // the SSBO via gl_VertexID, generating a two-triangle quad per particle with no
  // bound vertex attributes. Dead particles produce zero-area degenerate triangles
  // that the rasteriser discards at no fragment cost.
  //
  // Blending is set to additive (GL_SRC_ALPHA, GL_ONE) during Render() and restored
  // afterwards, producing an HDR glow effect when particles overlap.
  class ParticleSystem {
  public:
    // Parameters for a single Emit call. All particles in one call share the same
    // base parameters; per-particle variation is applied via the spread fields.
    struct EmitParams {
      glm::vec2 position;
      glm::vec2 velocity;                 // base velocity (world units / second)
      float     velocitySpreadAngle = 0.3f; // ± radians added to base direction
      float     speedSpread         = 0.0f; // ± world units / second added to base speed
      glm::vec4 colorStart          = { 1.0f, 1.0f, 1.0f, 1.0f };
      glm::vec4 colorEnd            = { 1.0f, 1.0f, 1.0f, 0.0f };
      float     lifetimeMin         = 0.5f; // seconds
      float     lifetimeMax         = 1.0f;
      float     sizeStart           = 8.0f; // pixels at birth
      float     sizeEnd             = 0.0f; // pixels at death
      float     drag                = 2.0f; // exponential velocity decay rate
      int       count               = 1;
    };

    // maxParticles: total live particle capacity. Memory allocated upfront.
    // Must be a multiple of 256 (the compute shader local size).
    explicit ParticleSystem(int maxParticles = 16384);
    ~ParticleSystem();

    ParticleSystem(const ParticleSystem&)            = delete;
    ParticleSystem& operator=(const ParticleSystem&) = delete;

    // Emit particles with per-particle randomisation from rng.
    // Uses a ring buffer — if the pool is full the oldest particles are overwritten.
    void Emit(const EmitParams& params, std::mt19937& rng);

    // Dispatch the compute shader to advance all particles by dt seconds.
    // Call once per frame before Render().
    void Update(float dt);

    // Draw all alive particles into the currently bound framebuffer.
    // Temporarily sets additive blending and restores standard alpha blend on return.
    void Render(const OrthographicCamera& camera);

    int GetMaxParticles() const { return m_MaxParticles; }

  private:
    // GL object handles hidden from the header — avoids including glad.h here.
    struct GLHandles {
      uint32_t ssbo = 0; // GL_SHADER_STORAGE_BUFFER holding Particle array
      uint32_t vao  = 0; // Empty VAO required by Core Profile for draw calls
    };

    int        m_MaxParticles;
    int        m_NextSlot    = 0;    // Ring-buffer cursor; advances on every Emit
    GLHandles  m_GL;
    std::unique_ptr<class Shader> m_ComputeShader;
    std::unique_ptr<class Shader> m_RenderShader;
  };

} // namespace Marble
