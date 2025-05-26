#!/usr/bin/env bash

# ─── CONFIG ─────────────────────────────────────────────
SCRIPT="send_request.py"
SESSION="aaa"
# ────────────────────────────────────────────────────────


tmux set-option -g remain-on-exit off

CMD="bash -lc 'python3 \"$SCRIPT\"; \
  echo; echo \"[pane] Script finished — press any key to close this pane…\"; \
  read -n1'"


tmux new-session -d -s "$SESSION" "$CMD"


tmux split-window -h -t "$SESSION" "$CMD"

tmux attach -t "$SESSION"
