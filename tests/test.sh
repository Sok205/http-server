#!/usr/bin/zsh

# ─── CONFIG ─────────────────────────────────────────────
SCRIPT="send_request.py"
SESSION="aaa"
# ────────────────────────────────────────────────────────

wait_30_seconds() {
  echo "Waiting for 30 seconds…"
  sleep 30
  echo "Done waiting."
}

tmux set-window-option -t "$SESSION" remain-on-exit on


CMD="bash -lc 'python3 \"$SCRIPT\"; \
  echo; echo \"[pane] Script finished — press any key to close this pane…\"; \
  read -n1'"


tmux new-session -d -s "$SESSION" "$CMD"


tmux split-window -h -t "$SESSION" "$CMD"

tmux attach -t "$SESSION"



