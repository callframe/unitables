import sys
from pathlib import Path

# Arguments in expected order:
# - Output directory
# - Path to UnicodeData.txt

if len(sys.argv) != 3:
    print("Usage: python unitables.py <output_dir> <unicode_data_path>")
    sys.exit(1)

OUTPUT_DIR = Path(sys.argv[1])
UNICODE_DATA_PATH = Path(sys.argv[2])

# Output file: unitables_data.c

UNITABLES_DATA_C = OUTPUT_DIR / "unitables_data.c"
