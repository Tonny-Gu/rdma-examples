#!/usr/bin/env bash

# Hook to log token usage to ~/claude-usage.log
# Triggered on Stop event (after Claude finishes responding)

# Debug log file
DEBUG_LOG=~/claude-hook-debug.log

# Read hook input JSON from stdin
INPUT=$(cat)

# Require a JSON parser for hook input
have_jq=0
have_py=0
if command -v jq >/dev/null 2>&1; then
    have_jq=1
fi
if command -v python3 >/dev/null 2>&1; then
    have_py=1
fi
if [ "$have_jq" -eq 0 ] && [ "$have_py" -eq 0 ]; then
    echo "ERROR: jq or python3 is required to parse hook input" >> "$DEBUG_LOG"
    exit 0
fi

get_input_field() {
    local key="$1"
    if [ "$have_py" -eq 1 ]; then
        HOOK_INPUT="$INPUT" HOOK_KEY="$key" python3 - <<'PY'
import json
import os
import sys

raw = os.environ.get("HOOK_INPUT", "")
key = os.environ.get("HOOK_KEY", "")
try:
    data = json.loads(raw)
except Exception:
    sys.exit(0)
val = data.get(key, "")
if val is None:
    val = ""
print(val)
PY
    else
        echo "$INPUT" | jq -r --arg key "$key" '.[$key] // empty'
    fi
}

extract_usage() {
    local path="$1"
    if [ "$have_py" -eq 1 ]; then
        TRANSCRIPT_PATH="$path" python3 - 2>>"$DEBUG_LOG" <<'PY'
import json
import os
import sys
from collections import deque

path = os.environ.get("TRANSCRIPT_PATH", "")
try:
    with open(path, "r", encoding="utf-8") as f:
        lines = deque(f, maxlen=50)
except Exception as e:
    print("null")
    print(f"ERROR: failed reading transcript: {e}", file=sys.stderr)
    sys.exit(0)

last = None
for line in lines:
    line = line.strip()
    if not line:
        continue
    try:
        obj = json.loads(line)
    except Exception:
        continue
    usage = obj.get("message", {}).get("usage")
    if usage is not None:
        last = usage

if last is None:
    print("null")
    sys.exit(0)

def get(name):
    value = last.get(name, 0)
    return 0 if value is None else value

print(
    f"input={get('input_tokens')} "
    f"cache_creation={get('cache_creation_input_tokens')} "
    f"cache_read={get('cache_read_input_tokens')} "
    f"output={get('output_tokens')}"
)
PY
    else
        tail -50 "$path" | jq -rs '
          map(select(.message.usage != null))
          | last
          | .message.usage
          | {
              input: (.input_tokens // 0),
              cache_creation: (.cache_creation_input_tokens // 0),
              cache_read: (.cache_read_input_tokens // 0),
              output: (.output_tokens // 0)
            }
          | "input=\(.input) cache_creation=\(.cache_creation) cache_read=\(.cache_read) output=\(.output)"
        ' 2>>"$DEBUG_LOG"
    fi
}

# Debug: log the input
echo "[$(date '+%Y-%m-%d %H:%M:%S')] Stop hook triggered" >> "$DEBUG_LOG"
echo "Input: $INPUT" >> "$DEBUG_LOG"

# Extract transcript path from JSON
TRANSCRIPT_PATH=$(get_input_field transcript_path)

echo "Transcript path: $TRANSCRIPT_PATH" >> "$DEBUG_LOG"

if [ -z "$TRANSCRIPT_PATH" ]; then
    echo "ERROR: transcript_path is empty" >> "$DEBUG_LOG"
    exit 0
fi

if [ ! -f "$TRANSCRIPT_PATH" ]; then
    echo "ERROR: transcript file does not exist: $TRANSCRIPT_PATH" >> "$DEBUG_LOG"
    exit 0
fi

echo "Transcript file exists, processing..." >> "$DEBUG_LOG"

# Extract token usage from the last message with usage information
LAST_USAGE=$(extract_usage "$TRANSCRIPT_PATH")

echo "Extracted token info: $LAST_USAGE" >> "$DEBUG_LOG"

# If we found token usage info, log it
if [ -n "$LAST_USAGE" ] && [ "$LAST_USAGE" != "null" ]; then
    TIMESTAMP=$(date '+%Y-%m-%d %H:%M:%S')
    SESSION_ID=$(get_input_field session_id)
    if [ -z "$SESSION_ID" ]; then
        SESSION_ID="unknown"
    fi
    CWD=$(get_input_field cwd)
    if [ -z "$CWD" ]; then
        CWD="unknown"
    fi

    # Append to log file
    echo "$TIMESTAMP | session=$SESSION_ID | cwd=$CWD | $LAST_USAGE" >> ~/claude-usage.log
    echo "Successfully logged to ~/claude-usage.log" >> "$DEBUG_LOG"
else
    echo "ERROR: No token info found in transcript" >> "$DEBUG_LOG"
fi
