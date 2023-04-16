import numpy as np
import matplotlib.pyplot as plt
from pathlib import Path

def load_raw_data(path, flip_y=False):
    parts = Path(path).name.split('.')
    fmt, w, h, ext = parts[-4:]
    w, h = int(w), int(h)
    assert fmt == 'f32'
    with open(path, 'rb') as f:
        data = np.frombuffer(f.read(), dtype=np.float32)
    data = data.reshape((h, w, 4))
    if flip_y:
        data = data[::-1,:,:] # flip y
    return data

def parse_crop_xy_from_png_path(path):
    if 'cropx' in path:
        x, y = Path(path).stem.split("_")[-2:]
        assert int(x) % 2 == 0 and int(x) % 2 == 0, "'cropx' style image crop coords must be divisible by two"
        return int(x)//2, int(y)//2
    elif 'crop' in path:
        x, y = Path(path).stem.split("_")[-2:]
        return int(x), int(y)
    else:
        return 0, 0


if __name__ == "__main__":
    if False:
        path = 'images/painted/spheres1.f32.960.540.data'
        data = load_raw_data(path)
        print(f"data shape: {data.shape}")
        plt.imshow(data)
        plt.show()
    
    x, y = parse_crop_xy_from_png_path('images/painted/spheres1_mucha1_crop_250_20.png')
    print(x,y)
    assert x == 250
    assert y == 20