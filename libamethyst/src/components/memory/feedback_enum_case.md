---
name: feedback-enum-case
description: Enum values in this codebase use CAPITAL_CASE, not CamelCase
metadata:
  type: feedback
---

Enum values must be CAPITAL_CASE (e.g. `State::CLOSED`, not `State::Closed`).

**Why:** Matches the project's established naming conventions (CLAUDE.md: "Enum values: CAPITAL_CASE").

**How to apply:** Whenever writing a new enum class or enum values, always use ALL_CAPS for the values. This applies to all enums, including private inner enums.
