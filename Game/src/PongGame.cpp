#include "PongGame.h"
#include <cmath>
#include <string>

// ── Layout constants ─────────────────────────────────────────────────────────
static const glm::vec2 PADDLE_SIZE  = { 20.0f, 140.0f };
static const glm::vec2 BALL_SIZE    = { 18.0f,  18.0f };

// How far from the edge the paddle center sits
static constexpr float PADDLE_MARGIN = 60.0f;

// ── Gameplay constants ────────────────────────────────────────────────────────
static constexpr float PLAYER_SPEED       = 550.0f; // px/s
static constexpr float AI_SPEED           = 420.0f; // px/s (slightly beatable)
static constexpr float BALL_SPEED_INITIAL = 420.0f; // px/s on launch
static constexpr float BALL_SPEED_MAX     = 900.0f; // cap to keep it playable
static constexpr float BALL_SPEED_INC     =  35.0f; // added per paddle bounce
static constexpr float RESET_DELAY        =   1.2f; // seconds of pause after goal

// ── Visual constants ──────────────────────────────────────────────────────────
static constexpr float DASH_W   = 6.0f;
static constexpr float DASH_H   = 24.0f;
static constexpr float DASH_GAP = 14.0f;

static constexpr float SCORE_FONT_SIZE = 80.0f; // px, DroidSans looks great here
static constexpr float SCORE_Y_FRAC    = 0.87f; // fraction of RH for baseline

// ─────────────────────────────────────────────────────────────────────────────

void PongGame::OnStart() {
  m_RW = static_cast<float>(App().GetRenderWidth());
  m_RH = static_cast<float>(App().GetRenderHeight());

  m_Camera = std::make_unique<Marble::OrthographicCamera>(0.0f, m_RW, 0.0f, m_RH);
  m_HUD    = std::make_unique<Marble::HUD>(static_cast<int>(m_RW), static_cast<int>(m_RH));
  m_Font   = std::make_unique<Marble::Font>("assets/fonts/DroidSans.ttf", SCORE_FONT_SIZE);

  // Subtle post-process — keep it clean for Pong
  Marble::PostProcessSettings& pp = App().GetPostProcessSettings();
  pp.VignetteStrength             = 0.15f;
  pp.Contrast                     = 1.05f;

  // Initial paddle positions
  m_PlayerPos = { PADDLE_MARGIN,        m_RH * 0.5f };
  m_AIPos     = { m_RW - PADDLE_MARGIN, m_RH * 0.5f };

  ResetBall(0); // launch toward player first
}

void PongGame::OnUpdate(float deltaTime) {
  if (Marble::Input::IsKeyJustPressed(Marble::Key::F11)) {
    App().SetFullscreen(!App().IsFullscreen());
  }

  // ── Goal pause ────────────────────────────────────────────────────────────
  if (m_Paused) {
    m_ResetTimer -= deltaTime;
    if (m_ResetTimer <= 0.0f) m_Paused = false;
    return;
  }

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
  m_AIPos.y = glm::clamp(m_AIPos.y + aiMove, halfPH, m_RH - halfPH);

  // ── Ball movement ─────────────────────────────────────────────────────────
  m_BallPos += m_BallVel * deltaTime;

  const float halfBW = BALL_SIZE.x * 0.5f;
  const float halfBH = BALL_SIZE.y * 0.5f;

  // Wall bounce (top / bottom)
  if (m_BallPos.y - halfBH < 0.0f) {
    m_BallPos.y = halfBH;
    m_BallVel.y = std::abs(m_BallVel.y);
  }
  if (m_BallPos.y + halfBH > m_RH) {
    m_BallPos.y = m_RH - halfBH;
    m_BallVel.y = -std::abs(m_BallVel.y);
  }

  // ── Paddle collision ──────────────────────────────────────────────────────
  const Marble::AABB ballBox   = Marble::AABB::FromCenter(m_BallPos,   BALL_SIZE);
  const Marble::AABB playerBox = Marble::AABB::FromCenter(m_PlayerPos, PADDLE_SIZE);
  const Marble::AABB aiBox     = Marble::AABB::FromCenter(m_AIPos,     PADDLE_SIZE);

  auto bounceOffPaddle = [&](const Marble::AABB& paddle, float outwardDir) {
    if (!ballBox.Overlaps(paddle)) return;
    if (m_BallVel.x * outwardDir >= 0.0f) return; // already moving away

    // Increase speed on each bounce, capped at max
    const float speed   = glm::min(glm::length(m_BallVel) + BALL_SPEED_INC, BALL_SPEED_MAX);
    const float hitFrac = (m_BallPos.y - paddle.Center.y) / paddle.HalfSize.y; // [-1, 1]
    const float angle   = hitFrac * glm::radians(60.0f); // ±60° deflection

    m_BallVel.x = outwardDir * speed * std::cos(angle);
    m_BallVel.y =              speed * std::sin(angle);

    // Push ball clear of the paddle to prevent tunnelling
    if (outwardDir > 0)
      m_BallPos.x = paddle.Max().x + halfBW;
    else
      m_BallPos.x = paddle.Min().x - halfBW;
  };

  bounceOffPaddle(playerBox, +1.0f); // player is on the left -> deflect rightward
  bounceOffPaddle(aiBox,     -1.0f); // AI is on the right    -> deflect leftward

  // ── Scoring ───────────────────────────────────────────────────────────────
  if (m_BallPos.x < 0.0f) {
    ++m_AIScore;
    ResetBall(-1);
  } else if (m_BallPos.x > m_RW) {
    ++m_PlayerScore;
    ResetBall(+1);
  }
}

void PongGame::OnRender(Marble::Renderer2D& r) {
  r.BeginScene(*m_Camera);
  DrawCenterLine(r);
  r.DrawQuad(m_PlayerPos, PADDLE_SIZE, Marble::Colors::White);
  r.DrawQuad(m_AIPos,     PADDLE_SIZE, Marble::Colors::White);
  r.DrawQuad(m_BallPos,   BALL_SIZE,   Marble::Colors::White);
  r.EndScene();

  // HUD pass - screen-space, unaffected by the world camera
  m_HUD->BeginRender(r);
  DrawScore(r);
  m_HUD->EndRender(r);
}

void PongGame::OnStop() {
  m_Font.reset();
  m_HUD.reset();
  m_Camera.reset();
}

void PongGame::OnResize() {
  m_RW = static_cast<float>(App().GetRenderWidth());
  m_RH = static_cast<float>(App().GetRenderHeight());
  m_Camera->SetProjection(0.0f, m_RW, 0.0f, m_RH);
  m_HUD->Resize(static_cast<int>(m_RW), static_cast<int>(m_RH));
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

  m_BallVel = {
    dirX  * BALL_SPEED_INITIAL * std::cos(angle),
             BALL_SPEED_INITIAL * std::sin(angle),
  };

  m_Paused     = true;
  m_ResetTimer = RESET_DELAY;
}

void PongGame::DrawCenterLine(Marble::Renderer2D& r) const {
  static constexpr Marble::Color LINE_COLOR = { 1.0f, 1.0f, 1.0f, 0.25f };

  const float x       = m_RW * 0.5f;
  const float totalH  = DASH_H + DASH_GAP;
  const int   dashes  = static_cast<int>(m_RH / totalH) + 1;

  for (int i = 0; i < dashes; i++) {
    const float centerY = (i + 0.5f) * totalH - DASH_GAP * 0.5f;
    r.DrawQuad({ x, centerY }, { DASH_W, DASH_H }, LINE_COLOR);
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
