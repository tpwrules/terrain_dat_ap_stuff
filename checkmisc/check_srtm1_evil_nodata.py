import numpy as np
import sys
import zipfile

def check(f):
    with zipfile.ZipFile(f) as zf:
        with zf.open(zf.infolist()[0]) as df:
            d = np.frombuffer(df.read(), dtype='>i2')
            dim = int(len(d)**0.5)
            d = d.reshape(dim, dim)
            bad = d == -32768
            if np.any(bad):
                if np.any(bad[1:, :]): # only ever seen in first row
                    print(f) # problem in subsequent rows!
                elif np.any(d[1, bad[0]] != 0): # below value not zero?
                    print(f) # big problem!

def do(f):
    try:
        check(f)
    except Exception:
        import traceback
        print(f, file=sys.stderr)
        traceback.print_exc()

for ff in sys.argv[1:]: do(ff)
