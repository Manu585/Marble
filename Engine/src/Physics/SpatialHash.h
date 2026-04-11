#pragma once
#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "Collision.h"

namespace Marble {

// ── SpatialHashGrid ────────────────────────────────────────────────────────────
//
// Broad-phase uniform-grid spatial hash for 2D AABB collision detection.
//
// Problem it solves
// -----------------
// Checking every object against every other is O(n²). For 10 objects that's
// 45 checks; for 500 objects it's 124,750. A spatial hash reduces this to
// O(n · k), where k is the average number of objects sharing a cell — typically
// a small constant regardless of scene size.
//
// How it works
// ------------
// The world is divided into an infinite uniform grid of square cells. Each frame:
//   1. Clear() resets the grid.
//   2. Insert() places each object's index into every cell its AABB overlaps.
//   3. QueryPairs() iterates cells and reports each pair that shares a cell —
//      these are your collision *candidates*. Run the exact AABB::Overlaps()
//      check (narrow phase) inside the callback to confirm real overlaps.
//
// Typical per-frame usage
// -----------------------
//   m_Grid.Clear();
//   for (auto& e : m_Entities)
//       m_Grid.Insert(e.id, e.aabb);
//
//   m_Grid.QueryPairs([&](int a, int b) {
//       if (m_Entities[a].aabb.Overlaps(m_Entities[b].aabb))
//           ResolveCollision(a, b);
//   });
//
// Choosing a cell size
// --------------------
//   Too small → many empty cells, excess memory and hash overhead.
//   Too large → many objects per cell, degrades toward O(n²).
//   Rule of thumb: 1.5–2× the average object diameter works well for most games.
//
// Template parameter
// ------------------
//   Handle — the type used to identify objects (default: int). Must be copyable.
//   Internally, objects are tracked by insertion index so duplicate Handle values
//   or even the same object inserted twice are handled correctly.

template<typename Handle = int>
class SpatialHashGrid {
public:
  // Construct with a fixed cell size (in world units, e.g. pixels).
  explicit SpatialHashGrid(float cellSize)
    : m_CellSize(cellSize)
  {
    // Reserve some upfront capacity to avoid rehashing on the first few frames.
    m_Cells.reserve(256);
    m_Objects.reserve(256);
  }

  // ── Per-frame lifecycle ─────────────────────────────────────────────────────

  // Reset the grid. Call once at the start of each frame, before any Insert().
  void Clear()
  {
    m_Cells.clear();
    m_Objects.clear();
  }

  // Register an object for this frame.
  // An object whose AABB spans k cells is inserted into all k cells.
  void Insert(Handle handle, const AABB& aabb)
  {
    // Assign a compact, unique index to this object.
    const uint32_t objectIdx = static_cast<uint32_t>(m_Objects.size());
    m_Objects.push_back(handle);

    int x0, y0, x1, y1;
    CellRange(aabb, x0, y0, x1, y1);

    for (int cy = y0; cy <= y1; ++cy) {
      for (int cx = x0; cx <= x1; ++cx) {
        m_Cells[PackCell(cx, cy)].push_back(objectIdx);
      }
    }
  }

  // ── Queries ─────────────────────────────────────────────────────────────────

  // Append to `out` every handle whose registered AABB could overlap `aabb`.
  // Results may contain duplicates when an object spans multiple cells that all
  // intersect `aabb`; callers should deduplicate or tolerate them.
  // Use this for single-object queries ("what might my bullet be hitting?").
  void Query(const AABB& aabb, std::vector<Handle>& out) const
  {
    int x0, y0, x1, y1;
    CellRange(aabb, x0, y0, x1, y1);

    for (int cy = y0; cy <= y1; ++cy) {
      for (int cx = x0; cx <= x1; ++cx) {
        auto it = m_Cells.find(PackCell(cx, cy));
        if (it == m_Cells.end()) continue;
        for (uint32_t idx : it->second) {
          out.push_back(m_Objects[idx]);
        }
      }
    }
  }

  // Invoke callback(handleA, handleB) for every unique candidate pair that
  // shares at least one cell. Each pair is reported exactly once.
  //
  // Callback signature:  void(Handle a, Handle b)
  //
  // This is the primary broad-phase interface — run your narrow-phase
  // AABB::Overlaps() check inside the callback to confirm real collisions.
  template<typename Fn>
  void QueryPairs(Fn&& callback) const
  {
    // We deduplicate by packing the two uint32_t object indices into one
    // uint64_t key. Each Insert() produces a unique index so this is safe
    // even when multiple objects share the same Handle value.
    std::unordered_set<uint64_t> seen;

    for (auto& [cellKey, indices] : m_Cells) {
      for (std::size_t i = 0; i < indices.size(); ++i) {
        for (std::size_t j = i + 1; j < indices.size(); ++j) {
          const uint32_t lo = (indices[i] < indices[j]) ? indices[i] : indices[j];
          const uint32_t hi = (indices[i] < indices[j]) ? indices[j] : indices[i];

          const uint64_t pairKey = (static_cast<uint64_t>(lo) << 32) | hi;
          if (seen.insert(pairKey).second) {
            callback(m_Objects[lo], m_Objects[hi]);
          }
        }
      }
    }
  }

private:
  float m_CellSize;

  // Maps packed cell coordinates → sorted list of object indices.
  std::unordered_map<uint64_t, std::vector<uint32_t>> m_Cells;

  // Insertion-ordered handle registry. Index is the object's stable ID inside
  // the grid for this frame.
  std::vector<Handle> m_Objects;

  // ── Coordinate helpers ──────────────────────────────────────────────────────

  // Convert a world coordinate to its containing cell index (signed).
  int WorldToCell(float v) const
  {
    return static_cast<int>(std::floor(v / m_CellSize));
  }

  // Compute the inclusive min/max cell range that covers `aabb`.
  void CellRange(const AABB& aabb, int& x0, int& y0, int& x1, int& y1) const
  {
    const glm::vec2 mn = aabb.Min();
    const glm::vec2 mx = aabb.Max();
    x0 = WorldToCell(mn.x);
    y0 = WorldToCell(mn.y);
    x1 = WorldToCell(mx.x);
    y1 = WorldToCell(mx.y);
  }

  // Pack two signed cell coordinates into a single 64-bit map key.
  // Casting to uint32_t first gives well-defined wraparound for negative coords.
  static uint64_t PackCell(int cx, int cy)
  {
    return (static_cast<uint64_t>(static_cast<uint32_t>(cx)) << 32)
         |  static_cast<uint64_t>(static_cast<uint32_t>(cy));
  }
};

} // namespace Marble
