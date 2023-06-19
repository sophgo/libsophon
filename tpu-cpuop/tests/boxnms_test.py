import mxnet as m

x = m.ndarray.array([
    [[0, 0.5, 0.1, 0.1, 0.2, 0.2],
     [1, 0.4, 0.1, 0.1, 0.2, 0.2], 
     [0, 0.3, 0.1, 0.1, 0.14, 0.14],
     [2, 0.6, 0.5, 0.5, 0.7, 0.8]],

    [[3, 0.5, 0.1, 0.1, 0.2, 0.2],
     [1, 0.4, 0.1, 0.1, 0.2, 0.2], 
     [0, 0.3, 0.1, 0.1, 0.14, 0.14],
     [2, 0.25, 0.5, 0.5, 0.7, 0.8]]])

y = m.ndarray.contrib.box_nms(
    x,
    overlap_thresh=0.1,
    valid_thresh = 0.35,
    coord_start=2,
    score_index=1,
    id_index=0,
    force_suppress=True,
    in_format="corner",
    #background_id=2, #not supported by mxnet
    out_format="center"
)

print(x)
print(y)
