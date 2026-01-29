from terrain_gen import create_degree_map

latlon_e7_vals, meter_vals, error_vals = create_degree_map(60, -90, 50, "4.1")

import numpy as np
ev = np.asarray(error_vals)
import matplotlib.pyplot as plt
plt.hist(ev[:, 1])
plt.show()
