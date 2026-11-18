"""Pack the five supplied pictures as RGB565 assets, preserving their artwork.

This performs only image-format conversion and proportional downsampling.
Backgrounds and stock watermarks are deliberately retained. No network upload.
Outputs a JSON object containing an Arduino header for apply_patch integration.
Requires Pillow. Run with the five input paths in car/obstacle/dragon/princess/hero order.
"""
import json
import sys
from PIL import Image, ImageOps

SPECS = [('CAR', (44, 60)), ('OBSTACLE', (44, 44)),
         ('DRAGON', (128, 136)), ('PRINCESS', (128, 136)),
         ('HERO', (128, 136))]

def convert(paths):
    if len(paths) != len(SPECS):
        raise ValueError('Supply exactly five artwork paths')
    lines = ['#pragma once', '#include <Arduino.h>',
             'struct Sprite { int width; int height; const uint16_t* pixels; };']
    for path, (name, bounds) in zip(paths, SPECS):
        im = ImageOps.exif_transpose(Image.open(path)).convert('RGBA')
        im = ImageOps.contain(im, bounds, Image.Resampling.LANCZOS)
        # Only actual alpha is composited; checkerboard pixels remain intact.
        bg = Image.new('RGBA', im.size, (255, 248, 224, 255))
        im = Image.alpha_composite(bg, im).convert('RGB')
        values = [((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
                  for r, g, b in im.getdata()]
        lines.append(f'const uint16_t {name}_PIXELS[] PROGMEM = {{')
        for i in range(0, len(values), 16):
            lines.append('  ' + ','.join(f'0x{v:04X}' for v in values[i:i+16]) + ',')
        lines.extend(['};', f'const Sprite {name} = {{{im.width}, {im.height}, {name}_PIXELS}};'])
    lines.append('')
    return '\n'.join(lines)

if __name__ == '__main__':
    print(json.dumps({'header': convert(sys.argv[1:])}))
