import mxnet as m
import numpy as np
x_height = 20
x_width = 20
x_shape = (2,3,x_height, x_width)
x_size= 1
for s in x_shape:
    x_size = x_size*s
x = m.ndarray.arange(x_size, dtype=np.float32).reshape(x_shape)
rois = m.ndarray.array([
    [0, 0,0, x_height, x_width/2],
    [1, 0,0, x_height/2, x_width],
])

y = m.ndarray.contrib.ROIAlign(x, rois=rois, pooled_size=(4,3), spatial_scale=1.0, sample_ratio=2)
print(y)