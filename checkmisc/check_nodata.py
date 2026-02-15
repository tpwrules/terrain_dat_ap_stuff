import numpy as np
import sys
import zipfile

def check(f):
    with zipfile.ZipFile(f) as zf:
        with zf.open(zf.infolist()[0]) as df:
            d = np.frombuffer(df.read(), dtype='>i2')
            if np.any(d == -32768):
                print(f)

def do(f):
    try:
        check(f)
    except Exception:
        import traceback
        print(f, file=sys.stderr)
        traceback.print_exc()

for ff in sys.argv[1:]: do(ff)
