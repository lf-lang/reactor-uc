import sys
from pathlib import Path
import pandas as pd

THIS_DIR = Path(__file__).parent.resolve()
SAMPLE_DIR = THIS_DIR / 'samples'
TEST_DIR = THIS_DIR / 'test'

def parse_report(path: Path) -> pd.DataFrame:
    # Read file and separate on whitespace
    # Also skip every other line except for the first one to only get
    # the header once
    df = pd.read_csv(path, sep=r'\s+',
        skiprows=lambda x: x % 2 == 0 and x > 1
    )

    # hex is dec just shown in hexadecimal - not useful especially because
    # pandas interprets them as strings
    df = df.drop('hex', axis=1)

    return df

NUM_COLS = ['text', 'data', 'bss', 'dec']

def find_percentages(rep1: pd.DataFrame, rep2: pd.DataFrame) -> pd.DataFrame:
    # Merge on filename so only common tests are compared
    merged = rep1.merge(rep2, on='filename', suffixes=('_update', '_main'))

    pct = pd.DataFrame()
    pct['filename'] = merged['filename']
    for col in NUM_COLS:
        pct[col] = merged[f'{col}_update'] / merged[f'{col}_main'] * 100 - 100

    return pct, merged

def format_basic_report(merged: pd.DataFrame, report: pd.DataFrame) -> str:
    ret = ""
    for idx, row in report.iterrows():
        ret += f"Test {idx}: {row['filename'].replace('_c', '.c')}:\n"
        m = merged.loc[idx]
        for name in NUM_COLS:
            ret += f"% {name : <4}: {row[name] : <8.2f} (from {m[f'{name}_main'] : >6} -> {m[f'{name}_update'] : >6})\n"
        ret += '\n'
    return ret

def format_md_tables(merged: pd.DataFrame, report: pd.DataFrame) -> str:
    format_string = ""
    for idx, row in report.iterrows():
        m = merged.loc[idx]
        table = f"""
|    | from | to | increase (%) |
| -- | ---- | ------ | ------------ |
"""
        for col in NUM_COLS:
            table += f"| {col : <4} | {m[f'{col}_main']} | {m[f'{col}_update']} | {row[col] : <4.2f} |\n"
        format_string += f"## {row['filename']}\n" + table + "\n\n"

    return format_string

TESTING = False
if TESTING:
    # For testing / development purposes
    update = parse_report(TEST_DIR / 'update.txt')
    main = parse_report(TEST_DIR / 'main.txt')
else:
    if len(sys.argv) != 4:
        print(f'Usage: {sys.argv[0]} <path to update report> <path to main report> <path to output>')
        exit(1)

    # Get reports in raw format
    update = parse_report(sys.argv[1])
    main = parse_report(sys.argv[2])
    out = sys.argv[3]

# Calculate percentage differences (only for tests present in both branches)
cmp, merged = find_percentages(update, main)

# Format a human readable report with added statistics
str = format_basic_report(merged, cmp)

if TESTING:
    # Check that the string is formatted as expected
    with open(TEST_DIR / 'expected.txt') as f:
        exp = f.read()
    assert(str == exp)
    print("Test ok.")
else:
    # Write the string
    with open(out + ".txt", 'w') as f:
        f.write(str)

    # Write table to string
    with open(out + ".md", 'w') as f:
        explanation = \
"""
Memory usage after merging this PR will be:
<details><summary>Memory Report</summary>

"""
        tables = format_md_tables(merged, cmp)
        f.write(explanation + "\n\n" + tables)
    print('Write ok.')
