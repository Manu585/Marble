#include "PongGame.h"
#include <cmath>
#include <string>
#include <glm/gtc/constants.hpp>

// ── Layout constants ──────────────────────────────────────────────────────────
static constexpr glm::vec2 PADDLE_SIZE  = { 20.0f, 140.0f };
static constexpr glm::vec2 BALL_SIZE    = { 18.0f,  18.0f };

// How far from the edge the paddle center sits
static constexpr float PADDLE_MARGIN = 60.0f;

// ── Gameplay constants ────────────────────────────────────────────────────────
static constexpr float PLAYER_SPEED       = 550.0f; // px/s
static constexpr float AI_SPEED           = 420.0f; // px/s (slightly beatable)
static constexpr float BALL_SPEED_INITIAL = 420.0f; // px/s on launch
static constexpr float BALL_SPEED_MAX     = 900.0f; // cap to keep it playable
static constexpr float BALL_SPEED_INC     =  35.0f; // added per paddle bounce
static constexpr float RESET_DELAY        =   1.2f; // seconds of pause after goal

// ── Visual constants ───────────────────────────────────────────────────────────
static constexpr float DASH_W   = 6.0f;
static constexpr float DASH_H   = 24.0f;
static constexpr float DASH_GAP = 14.0f;

static constexpr float SCORE_FONT_SIZE = 80.0f; // px, DroidSans looks great here
static constexpr float SCORE_Y_FRAC    = 0.87f; // fraction of RH for baseline

// ───────────────────────────────────────────────────────────────────────────────

void PongGame::OnStart() {
  m_RW = static_cast<float>(App().GetRenderWidth());
  m_RH = static_cast<float>(App().GetRenderHeight());

  m_Camera    = std::make_unique<Marble::OrthographicCamera>(0.0f, m_RW, 0.0f, m_RH);
  m_Font      = std::make_unique<Marble::Font>("assets/fonts/DroidSans.ttf", SCORE_FONT_SIZE);
  m_Particles = std::make_unique<Marble::ParticleSystem>(16384);

  m_SndPaddleHit = std::make_unique<Marble::Sound>("assets/audio/paddle_hit.wav");
  m_SndWallHit   = std::make_unique<Marble::Sound>("assets/audio/wall_hit.wav");
  m_SndScore     = std::make_unique<Marble::Sound>("assets/audio/score.wav");

  // Richer post-process to complement the additive particle glow
  Marble::PostProcessSettings& pp = App().GetPostProcessSettings();
  pp.VignetteStrength             = 0.30f;
  pp.Contrast                     = 1.08f;
  pp.Brightness                   = 0.95f;

  // Initial paddle positions
  m_PlayerPos = { PADDLE_MARGIN,        m_RH * 0.5f };
  m_AIPos     = { m_RW - PADDLE_MARGIN, m_RH * 0.5f };

  EmitBackgroundNebula();
  ResetBall(0); // launch toward player first
}

void PongGame::OnUpdate(float deltaTime) {
  if (Marble::Input::IsKeyJustPressed(Marble::Key::F11)) {
    App().SetFullscreen(!App().IsFullscreen());
  }

  // Always advance the particle simulation — background nebula drifts even
  // during the goal pause so the scene never looks frozen.
  m_Particles->Update(deltaTime);

  // ── Goal pause ────────────────────────────────────────────────────────────
  if (m_Paused) {
    m_ResetTimer -= deltaTime;
    if (m_ResetTimer <= 0.0f) m_Paused = false;
    return;
  }

  EmitBallTrail();

  // ── Player paddle (W / S  or  Up / Down) ──────────────────────────────────
  const float halfPH = PADDLE_SIZE.y * 0.5f;

  if (Marble::Input::IsKeyPressed(Marble::Key::W) ||
      Marble::Input::IsKeyPressed(Marble::Key::Up))
    m_PlayerPos.y += PLAYER_SPEED * deltaTime;

  if (Marble::Input::IsKeyPressed(Marble::Key::S) ||
      Marble::Input::IsKeyPressed(Marble::Key::Down))
    m_PlayerPos.y -= PLAYER_SPEED * deltaTime;

  m_PlayerPos.y = glm::clamp(m_PlayerPos.y, halfPH, m_RH - halfPH);

  // ── AI paddle (tracks ball Y with a speed cap) ────────────────────────────
  const float aiDelta = m_BallPos.y - m_AIPos.y;
  const float aiMove  = glm::clamp(aiDelta, -AI_SPEED * deltaTime, AI_SPEED * deltaTime);
  m_AIPos.y           = glm::clamp(m_AIPos.y + aiMove, halfPH, m_RH - halfPH);

  // ── Ball movement ─────────────────────────────────────────────────────────
  m_BallPos += m_BallVel * deltaTime;

  const float halfBW = BALL_SIZE.x * 0.5f;
  const float halfBH = BALL_SIZE.y * 0.5f;

  // Wall bounce (top / bottom)
  if (m_BallPos.y - halfBH < 0.0f) {
    m_BallPos.y = halfBH;
    m_BallVel.y = std::abs(m_BallVel.y);
    m_SndWallHit->Play();
  }
  if (m_BallPos.y + halfBH > m_RH) {
    m_BallPos.y = m_RH - halfBH;
    m_BallVel.y = -std::abs(m_BallVel.y);
    m_SndWallHit->Play();
  }

  // ── Paddle collision (broad phase → narrow phase) ────────────────────────
  //
  // Entity IDs match the constants in PongGame.h member comment:
  //   0 = player paddle,  1 = AI paddle,  2 = ball
  //
  // SpatialHashGrid::QueryPairs only fires the callback for pairs that share
  // at least one grid cell, so the narrow-phase Overlaps() is called only
  // when the ball is actually near a paddle — not every frame unconditionally.
  static constexpr int kPlayer = 0;
  static constexpr int kAI     = 1;
  static constexpr int kBall   = 2;

  const Marble::AABB ballBox   = Marble::AABB::FromCenter(m_BallPos,   BALL_SIZE);
  const Marble::AABB playerBox = Marble::AABB::FromCenter(m_PlayerPos, PADDLE_SIZE);
  const Marble::AABB aiBox     = Marble::AABB::FromCenter(m_AIPos,     PADDLE_SIZE);

  // Narrow-phase response — still an exact Overlaps() check, same logic as before.
  auto bounceOffPaddle = [&](const Marble::AABB& paddle, float outwardDir) {
    if (!ballBox.Overlaps(paddle)) return;
    if (m_BallVel.x * outwardDir >= 0.0f) return; // already moving away

    const float speed   = glm::min(glm::length(m_BallVel) + BALL_SPEED_INC, BALL_SPEED_MAX);
    const float hitFrac = (m_BallPos.y - paddle.Center.y) / paddle.HalfSize.y; // [-1, 1]
    const float angle   = hitFrac * glm::radians(60.0f);                        // ±60° deflection

    m_BallVel.x = outwardDir * speed * std::cos(angle);
    m_BallVel.y =              speed * std::sin(angle);

    // Push ball clear of the paddle to prevent tunnelling
    if (outwardDir > 0)
      m_BallPos.x = paddle.Max().x + halfBW;
    else
      m_BallPos.x = paddle.Min().x - halfBW;

    m_SndPaddleHit->Play();
    EmitPaddleSparks(m_BallPos, m_BallVel);
  };

  // Broad phase — insert all three objects and let the grid find candidates.
  m_Grid.Clear();
  m_Grid.Insert(kPlayer, playerBox);
  m_Grid.Insert(kAI,     aiBox);
  m_Grid.Insert(kBall,   ballBox);

  // QueryPairs fires only for pairs that share a grid cell.
  // We only care about ball↔paddle pairs; player↔AI never collide.
  const Marble::AABB* paddles[2]    = { &playerBox, &aiBox };
  const float         outwardDir[2] = { +1.0f,      -1.0f  };

  m_Grid.QueryPairs([&](int a, int b) {
    const bool aIsBall = (a == kBall);
    const bool bIsBall = (b == kBall);
    if (!aIsBall && !bIsBall) return;          // player↔AI — skip
    const int paddleId = aIsBall ? b : a;
    bounceOffPaddle(*paddles[paddleId], outwardDir[paddleId]);
  });

  // ── Scoring ───────────────────────────────────────────────────────────────
  if (m_BallPos.x < 0.0f) {
    ++m_AIScore;
    m_SndScore->Play();
    EmitScoreExplosion(m_BallPos);
    ResetBall(-1);
  } else if (m_BallPos.x > m_RW) {
    ++m_PlayerScore;
    m_SndScore->Play();
    EmitScoreExplosion(m_BallPos);
    ResetBall(+1);
  }
}

void PongGame::OnRender(Marble::Renderer2D& r) {
  // Particles drawn first so game objects composite on top with standard blending.
  m_Particles->Render(*m_Camera);

  r.BeginScene(*m_Camera);
  DrawCenterLine(r);
  r.DrawQuad(m_PlayerPos, PADDLE_SIZE, Marble::Colors::White);
  r.DrawQuad(m_AIPos,     PADDLE_SIZE, Marble::Colors::White);
  r.DrawQuad(m_BallPos,   BALL_SIZE,   Marble::Colors::White);
  r.EndScene();

  // Debug hitboxes — rendered as wireframe rects, zero cost in release builds.
  Marble::DebugDraw::Rect(Marble::AABB::FromCenter(m_PlayerPos, PADDLE_SIZE), Marble::Colors::Green);
  Marble::DebugDraw::Rect(Marble::AABB::FromCenter(m_AIPos,     PADDLE_SIZE), Marble::Colors::Red);
  Marble::DebugDraw::Rect(Marble::AABB::FromCenter(m_BallPos,   BALL_SIZE),   Marble::Colors::Cyan);
  Marble::DebugDraw::Flush(*m_Camera);
}

void PongGame::OnHudRender(Marble::Renderer2D& r) {
  DrawScore(r);
}

void PongGame::OnStop() {
  // Explicit resets are not strictly required — the destructor handles them.
  // They're here as a reminder that Sound objects must outlive Application
  // (which owns the audio engine). MARBLE_MAIN guarantees this via declaration
  // order: Application is declared before the GameLayer, so GameLayer (and its
  // Sounds) destruct first. The resets can safely be removed.
  m_SndPaddleHit.reset();
  m_SndWallHit.reset();
  m_SndScore.reset();
  m_Font.reset();
  m_Particles.reset();
  m_Camera.reset();
}

void PongGame::OnResize() {
  m_RW = static_cast<float>(App().GetRenderWidth());
  m_RH = static_cast<float>(App().GetRenderHeight());
  m_Camera->SetProjection(0.0f, m_RW, 0.0f, m_RH);
}

// ── Private helpers ───────────────────────────────────────────────────────────

void PongGame::ResetBall(int scoringSide) {
  m_BallPos = { m_RW * 0.5f, m_RH * 0.5f };

  // Launch toward the side that did NOT score (or a random side on first launch).
  // scoringSide: 0=start, -1=left scored (AI), +1=right scored (player)
  const float dirX = (scoringSide >= 0) ? -1.0f : +1.0f;

  // Uniform random angle in ±35° so every launch is unpredictable.
  std::uniform_real_distribution<float> angleDist(-35.0f, 35.0f);
  const float angle = glm::radians(angleDist(m_Rng));

  m_BallVel = { dirX  * BALL_SPEED_INITIAL * std::cos(angle),
                BALL_SPEED_INITIAL * std::sin(angle), };

  m_Paused     = true;
  m_ResetTimer = RESET_DELAY;
}

void PongGame::DrawCenterLine(Marble::Renderer2D& r) const {
  static constexpr Marble::Color LINE_COLOR = { 1.0f, 1.0f, 1.0f, 0.25f };

  const float x      = m_RW * 0.5f;
  const float totalH = DASH_H + DASH_GAP;
  const int   dashes = static_cast<int>(m_RH / totalH) + 1;

  for (int i = 0; i < dashes; i++) {
    const float centerY = (i + 0.5f) * totalH - DASH_GAP * 0.5f;
    r.DrawQuad({ x, centerY }, { DASH_W, DASH_H }, LINE_COLOR);
  }
}

// ── Particle emit helpers ─────────────────────────────────────────────────────

void PongGame::EmitBallTrail() {
  // Short-lived white-to-cyan sparks that trace the ball's path.
  // Emitted every frame; low count keeps it subtle rather than overwhelming.
  Marble::ParticleSystem::EmitParams p;
  p.position            = m_BallPos;
  // Trail emits opposite to travel direction so it streams behind the ball.
  p.velocity            = -m_BallVel * 0.12f;
  p.velocitySpreadAngle = glm::pi<float>() * 0.5f; // ±90° spread
  p.speedSpread         = 30.0f;
  p.colorStart          = { 1.0f, 1.0f, 1.0f, 0.85f };
  p.colorEnd            = { 0.2f, 0.8f, 1.0f, 0.0f };
  p.lifetimeMin         = 0.08f;
  p.lifetimeMax         = 0.22f;
  p.sizeStart           = 7.0f;
  p.sizeEnd             = 0.0f;
  p.drag                = 5.0f;
  p.count               = 4;
  m_Particles->Emit(p, m_Rng);
}

void PongGame::EmitPaddleSparks(glm::vec2 pos, glm::vec2 ballVel) {
  // Bright burst in the outgoing ball direction — hot white core fading to orange.
  Marble::ParticleSystem::EmitParams p;
  p.position            = pos;
  p.velocity            = ballVel * 0.55f;
  p.velocitySpreadAngle = glm::radians(55.0f);
  p.speedSpread         = 120.0f;
  p.colorStart          = { 1.0f, 1.0f, 0.85f, 1.0f };
  p.colorEnd            = { 1.0f, 0.35f, 0.0f, 0.0f };
  p.lifetimeMin         = 0.25f;
  p.lifetimeMax         = 0.55f;
  p.sizeStart           = 12.0f;
  p.sizeEnd             = 0.0f;
  p.drag                = 3.5f;
  p.count               = 90;
  m_Particles->Emit(p, m_Rng);
}

void PongGame::EmitScoreExplosion(glm::vec2 pos) {
  // Large omnidirectional burst. Three separate emits give colour variety:
  // a bright white core, a cyan mid-ring, and an orange outer spray.
  std::uniform_real_distribution<float> xDist(m_RW * 0.2f, m_RW * 0.8f);
  std::uniform_real_distribution<float> yDist(m_RH * 0.2f, m_RH * 0.8f);
  // Clamp pos to screen interior so the explosion is always visible.
  pos.x = glm::clamp(pos.x, m_RW * 0.1f, m_RW * 0.9f);
  pos.y = glm::clamp(pos.y, m_RH * 0.1f, m_RH * 0.9f);

  // White core — dense, fast, short-lived
  {
    Marble::ParticleSystem::EmitParams p;
    p.position            = pos;
    p.velocity            = { 0.0f, 0.0f };
    p.velocitySpreadAngle = glm::pi<float>();
    p.speedSpread         = 480.0f;
    p.colorStart          = { 1.0f, 1.0f, 1.0f, 1.0f };
    p.colorEnd            = { 1.0f, 1.0f, 1.0f, 0.0f };
    p.lifetimeMin         = 0.25f;
    p.lifetimeMax         = 0.50f;
    p.sizeStart           = 14.0f;
    p.sizeEnd             = 0.0f;
    p.drag                = 2.8f;
    p.count               = 120;
    m_Particles->Emit(p, m_Rng);
  }

  // Cyan ring — slower, larger, drifts outward
  {
    Marble::ParticleSystem::EmitParams p;
    p.position            = pos;
    p.velocity            = { 0.0f, 0.0f };
    p.velocitySpreadAngle = glm::pi<float>();
    p.speedSpread         = 260.0f;
    p.colorStart          = { 0.2f, 0.9f, 1.0f, 0.9f };
    p.colorEnd            = { 0.0f, 0.4f, 0.8f, 0.0f };
    p.lifetimeMin         = 0.55f;
    p.lifetimeMax         = 1.10f;
    p.sizeStart           = 18.0f;
    p.sizeEnd             = 2.0f;
    p.drag                = 1.6f;
    p.count               = 100;
    m_Particles->Emit(p, m_Rng);
  }

  // Orange embers — slow, survive longest, drift gently
  {
    Marble::ParticleSystem::EmitParams p;
    p.position            = pos;
    p.velocity            = { 0.0f, 60.0f }; // slight upward bias
    p.velocitySpreadAngle = glm::pi<float>();
    p.speedSpread         = 140.0f;
    p.colorStart          = { 1.0f, 0.5f, 0.1f, 0.8f };
    p.colorEnd            = { 0.8f, 0.1f, 0.0f, 0.0f };
    p.lifetimeMin         = 0.80f;
    p.lifetimeMax         = 1.60f;
    p.sizeStart           = 10.0f;
    p.sizeEnd             = 0.0f;
    p.drag                = 1.0f;
    p.count               = 80;
    m_Particles->Emit(p, m_Rng);
  }
}

void PongGame::EmitBackgroundNebula() {
  // Sparse, very large, very slow drifting particles that persist throughout
  // the match. They are barely visible individually but collectively create
  // a subtle depth and atmosphere behind the action.
  std::uniform_real_distribution<float> xDist(0.0f, m_RW);
  std::uniform_real_distribution<float> yDist(0.0f, m_RH);
  std::uniform_real_distribution<float> hueDist(0.0f, 1.0f);

  static constexpr int kNebula = 160;
  for (int i = 0; i < kNebula; ++i) {
    // Construct a random warm/cool colour for each nebula particle independently.
    const float hue = hueDist(m_Rng);
    glm::vec4 color;
    if (hue < 0.33f) {
      color = { 0.2f, 0.4f, 1.0f, 0.18f }; // blue
    } else if (hue < 0.66f) {
      color = { 0.6f, 0.1f, 0.8f, 0.14f }; // purple
    } else {
      color = { 0.0f, 0.7f, 0.6f, 0.12f }; // teal
    }

    Marble::ParticleSystem::EmitParams p;
    p.position            = { xDist(m_Rng), yDist(m_Rng) };
    p.velocity            = { 0.0f, 0.0f };
    p.velocitySpreadAngle = glm::pi<float>();
    p.speedSpread         = 18.0f;
    p.colorStart          = color;
    p.colorEnd            = { color.r, color.g, color.b, 0.0f };
    p.lifetimeMin         = 6.0f;
    p.lifetimeMax         = 12.0f;
    p.sizeStart           = 90.0f;
    p.sizeEnd             = 40.0f;
    p.drag                = 0.3f;
    p.count               = 1;
    m_Particles->Emit(p, m_Rng);
  }
}

void PongGame::DrawScore(Marble::Renderer2D& r) const {
  const float baselineY = m_RH * SCORE_Y_FRAC;
  const float midX      = m_RW * 0.5f;
  static constexpr float SCORE_SCALE = 1.0f; // font was baked at SCORE_FONT_SIZE already

  const std::string leftStr  = std::to_string(m_PlayerScore);
  const std::string rightStr = std::to_string(m_AIScore);

  const float leftW  = r.MeasureText(*m_Font, leftStr,  SCORE_SCALE);
  const float rightW = r.MeasureText(*m_Font, rightStr, SCORE_SCALE);

  // Draw player score slightly left of center, AI score to the right
  static constexpr float SCORE_OFFSET = 80.0f;
  r.DrawText(*m_Font, leftStr,  { midX - SCORE_OFFSET - leftW,  baselineY }, SCORE_SCALE, Marble::Colors::White);
  r.DrawText(*m_Font, rightStr, { midX + SCORE_OFFSET,          baselineY }, SCORE_SCALE, Marble::Colors::White);
}
