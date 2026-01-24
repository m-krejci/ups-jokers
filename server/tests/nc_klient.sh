#!/usr/bin/env bash
set -euo pipefail

PORT=10000
echo "Spouštím falešný server na portu $PORT..."

# Spusť nc jako koproces (obousměrně)
coproc NC { nc -l -p "$PORT"; }
nc_pid="${NC_PID:-}"

# Přečte přesně N bajtů z daného FD do proměnné (i když read někdy vrátí míň)
read_n() {
  local fd="$1" n="$2" out="" chunk=""
  while ((${#out} < n)); do
    # přečti zbytek do n; read může vrátit méně než chceš, proto loop
    IFS= read -r -n $((n - ${#out})) chunk <&"$fd" || return 1
    out+="$chunk"
  done
  printf '%s' "$out"
}

while true; do
  # 12B hlavička: JOKE + CMD(4) + LEN(4)
  header="$(read_n "${NC[0]}" 12)" || break
  magic="${header:0:4}"
  cmd="${header:4:4}"
  len_str="${header:8:4}"

  # základní kontrola
  if [[ "$magic" != "JOKE" ]]; then
    echo "Neplatná hlavička: [$header]"
    continue
  fi
  if [[ ! "$len_str" =~ ^[0-9]{4}$ ]]; then
    echo "Neplatná délka: [$len_str] v hlavičce [$header]"
    continue
  fi

  payload_len=$((10#$len_str))   # 10# => vždy dekadicky
  payload=""
  if ((payload_len > 0)); then
    payload="$(read_n "${NC[0]}" "$payload_len")" || break
  fi

  echo "KLIENT POSLAL: [${magic}${cmd}${len_str}] payload=[$payload]"

  case "$cmd" in
    LOGI)
      echo "--- Registruji login, posílám OKAY ---"
      printf "JOKEOKAY0004AHOJ" >&"${NC[1]}"
      ;;
    RCRT)
      echo "--- Klient chce vytvořit místnost ---"
      printf "JOKEOKAY0000" >&"${NC[1]}"
      sleep 3
      printf "JOKEGSTR0000" >&"${NC[1]}"
      ;;
    QUIT)
      echo "--- Klient se loučí ---"
      break
      ;;
    *)
      echo "--- Neznámý příkaz: $cmd ---"
      ;;
  esac
done

# Ukonči nc, ale nepadni když PID není
if [[ -n "$nc_pid" ]]; then
  kill "$nc_pid" 2>/dev/null || true
fi
echo "Server ukončen."
