#!/usr/bin/env bash
# check_data_hash.sh — Skip training if dataset is unchanged.
#
# Usage: check_data_hash.sh <data_dir> <output_dir> <python> <script> <output_dir> <data_dir>
#   $1  DATA_DIR    — dataset directory to hash
#   $2  OUTPUT_DIR  — build output directory (stores .data_hash)
#   $3  PYTHON      — .venv python interpreter
#   $4  SCRIPT      — training script (e.g. train.py)
#   $5  output_dir  — passed to train.py --output-dir
#   $6  data_dir    — passed to train.py --data-dir
set -euo pipefail

DATA_DIR="$1"
OUTPUT_DIR="$2"
PYTHON="$3"
SCRIPT="$4"
TRAIN_OUTPUT_DIR="$5"
TRAIN_DATA_DIR="$6"

HASH_FILE="${OUTPUT_DIR}/.data_hash"

# Hash all file paths + sizes (fast — no content reads).
NEW_HASH=$(find "$DATA_DIR" -type f | sort | xargs stat --format='%n %s' | md5sum | cut -d' ' -f1)

OLD_HASH=""
[ -f "$HASH_FILE" ] && OLD_HASH=$(cat "$HASH_FILE")

if [ "$NEW_HASH" = "$OLD_HASH" ]; then
    echo "Dataset unchanged (hash: ${NEW_HASH}). Skipping training."
    exit 0
fi

echo "Dataset changed (${OLD_HASH:-none} -> ${NEW_HASH}). Training..."
"$PYTHON" "$SCRIPT" --output-dir "$TRAIN_OUTPUT_DIR" --data-dir "$TRAIN_DATA_DIR"
mkdir -p "$OUTPUT_DIR"
echo "$NEW_HASH" > "$HASH_FILE"
