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
    df = pd.read_csv(path, sep='\s+', 
        skiprows=lambda x: x % 2 == 0 and x > 1
    )

    # hex is dec just shown in hexadecimal - not useful especially because
    # pandas interprets them as strings
    df = df.drop('hex', axis=1)

    return df

def find_percentages(rep1: pd.DataFrame, rep2: pd.DataFrame) -> pd.DataFrame:
    # Extract the integer values only
    data1 = rep1.iloc[:,0:4]
    data2 = rep2.iloc[:,0:4]

    # Calculate percentage increase/decrease
    return data1 / data2 * 100 - 100

def format_basic_report(update: pd.DataFrame, main: pd.DataFrame, report: pd.DataFrame) -> str:
    ret = ""
    for idx, row in report.iterrows():
        ret += f"Test {idx}: {row['filename'].replace('_c', '.c')}:\n"
        for name in report.head():
            if name != 'filename':
                ret += f"{name : <4}: {row[name] : <6.2f} (from {main.loc[idx][name] : >6} -> {update.loc[idx][name] : >6})\n"
        ret += '\n'


    return ret

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

# Calculate percentage differences
cmp = find_percentages(update, main)
cmp.insert(4, 'filename', update['filename'])

# Format a human readable report with added statistics
str = format_basic_report(update, main, cmp)

if TESTING:
    # Check that the string is formatted as expected
    with open(TEST_DIR / 'expected.txt') as f:
        exp = f.read()
    assert(str == exp)
    print("Test ok.")
else:
    # Write the string
    with open(out, 'w') as f:
        f.write(str)
    print('Write ok.')