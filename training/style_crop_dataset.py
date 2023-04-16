import os
import io
import PIL.Image, PIL.ImageDraw, PIL.ImageFont
import numpy as np
import matplotlib.pylab as pl
import torch
import random
from torch.utils.data.dataset import Dataset
import torchvision.transforms.functional

def expand_to_rgba(x):
  if x.shape[1] == 3:
    x2 = torch.zeros([1, 4, *x.shape[2:4]], device=x.device)
    x2[0,:3,:,:] = x
    return x2
  else:
    return x

class StyleCropDataset(Dataset):
    def __init__(self, input_img, target_img, style_func, activation_func, num_crops, crop_size):
        assert len(input_img.shape) == 3, "input image should have no batch dimension"
        assert input_img.shape[1] * 2 == target_img.shape[1]
        assert input_img.shape[2] * 2 == target_img.shape[2]

        target_img = target_img[:3,...]

        self.crops = []
        for _ in range(num_crops):
            h, w = crop_size, crop_size
            i = random.randint(0, input_img.shape[1] - h - 1)
            j = random.randint(0, input_img.shape[2] - w - 1)

            # i, j, h, w = params1
            input_crop = torchvision.transforms.functional.crop(input_img, i, j, h, w)
            target_crop = torchvision.transforms.functional.crop(target_img, i*2, j*2, h*2, w*2)

            assert len(target_crop.shape) == 3
            assert target_crop.shape[0] == 3

            # Gram matrices for style loss. Shape [1 x H x W]
            target_style = style_func(target_crop[None,...])
            assert(len(target_style[0].shape) == 3)

            # VGG-16 activations for content loss. Shape [1 x C x H x W]
            target_content = activation_func(target_crop[None,...])
            assert(len(target_content[0].shape) == 4)

            self.crops.append(
               {'input': input_crop,
                'target': target_crop,
                'target_style': target_style,
                'target_content': target_content})
        
    
    def __getitem__(self, index):
        return self.crops[index]
    
    def __len__(self):
       return len(self.crops)

