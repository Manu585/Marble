#pragma once
#include <Marble.h>
#include <memory>
#include <random>

class PongGame : public Marble::GameLayer {
public:
  void OnStart()                             override;
  void OnUpdate(float dt)                    override;
  void OnRender(Marble::Renderer2D& r)       override;
  void OnHudRender(Marble::Renderer2D& r)    override;
  void OnStop()                              override;
  void OnResize()                            override;

private:
  // ── Helpers ───────────────────────────────────────────────────────────────
  void ResetBall(int scoringSide); // -1 = left scored, +1 = right scored
  void DrawCenterLine(Marble::Renderer2D& r) const;
  void DrawScore(Marble::Renderer2D& r) const;

  // ── Particle helpers ─────────────────────────────────────────────────────
  void EmitBallTrail();
  void EmitPaddleSparks(glm::vec2 pos, glm::vec2 ballVel);
  void EmitScoreExplosion(glm::vec2 pos);
  void EmitBackgroundNebula();

  // ── Resources ─────────────────────────────────────────────────────────────
  std::unique_ptr<Marble::OrthographicCamera> m_Camera;
  std::unique_ptr<Marble::Font>               m_Font;
  std::unique_ptr<Marble::ParticleSystem>     m_Particles;

  std::unique_ptr<Marble::Sound> m_SndPaddleHit;
  std::unique_ptr<Marble::Sound> m_SndWallHit;
  std::unique_ptr<Marble::Sound> m_SndScore;

  // ── Game state ────────────────────────────────────────────────────────────
  float m_RW = 0.0f; // render width  (set in OnStart / OnResize)
  float m_RH = 0.0f; // render height

  glm::vec2 m_PlayerPos{};   // paddle center
  glm::vec2 m_AIPos{};       // paddle center

  glm::vec2 m_BallPos{};
  glm::vec2 m_BallVel{};     // pixels / second

  int m_PlayerScore = 0;
  int m_AIScore     = 0;

  // Brief pause after a goal before the ball re-launches
  float m_ResetTimer = 0.0f;
  bool  m_Paused     = false;

  std::mt19937 m_Rng{ std::random_device{}() };

  // Broad-phase spatial hash — cell size ≈ 2× average object diameter.
  // Entity IDs: 0 = player paddle, 1 = AI paddle, 2 = ball.
  Marble::SpatialHashGrid<int> m_Grid{ 150.0f };
};
