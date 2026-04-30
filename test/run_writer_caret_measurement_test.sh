#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SOURCE="$ROOT_DIR/src/activities/writer/WriterActivity.cpp"

measure_cursor_x_body="$(
  awk '
    /int WriterActivity::measureCursorX/ { in_fn = 1 }
    in_fn { print }
    in_fn && /^}/ { exit }
  ' "$SOURCE"
)"

if ! grep -q "getTextAdvanceX" <<<"$measure_cursor_x_body"; then
  echo "FAILED: measureCursorX should use glyph advance width for caret positioning"
  exit 1
fi

if grep -q "getTextWidth" <<<"$measure_cursor_x_body"; then
  echo "FAILED: measureCursorX should not use glyph bounds width for caret positioning"
  exit 1
fi

echo "WriterCaretMeasurementTest passed"
