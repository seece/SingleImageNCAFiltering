See `micro_nca_filtering.ipynb`

## Running the training code on Ubuntu 20.04

Install dependencies with [miniconda](https://docs.conda.io/en/latest/miniconda.html):

```
conda create --name myenv python=3.9
conda env update -n myenv -f renderer/environment.yml
```

Assuming CUDA 11.8, change the PyTorch package to match.
This also installs ffmpeg for video generation but it's optional.

You can also install packages manually:

```
conda activate myenv
conda install -y pytorch torchvision pytorch-cuda=11.8 -c pytorch -c nvidia
conda install -y tqdm
conda install -y -c conda-forge moviepy
conda install -y ipywidgets
conda install -y -c conda-forge ffmpeg # otherwise "-preset" option is missing from ffmpeg
conda install -y matplotlib
conda update -y ffmpeg
```

### Open the notebook in VSCode

Then in VSCode connect to your WSL instance and open the notebook. Make sure you have the VSCode Python extension installed *inside the WSL VM*. Your "myenv" Conda environment should be visible in the "Select Kernel" menu when opening the notebook. Click "Run all". Install the "ipykernel" package when prompted. 
