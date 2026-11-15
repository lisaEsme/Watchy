# Watch face design

- `face.png`: the design, 200 × 200 pixels, black and white only. This is the source for the
  watch face background.
- `sun.png`: the sun icon, 16 × 16, which the watch moves along the arc from sunrise to sunset.
- `moon.png`: the moon icon, which moves along the arc at night, from sunset (left) to sunrise (right).
- `icons/`: moon phases (64 × 64) and planet and zodiac symbols (16 × 16), drawn by the watch
  for the moon phase (big disc), the planetary hour (under the seven-pointed star) and the
  moon's sign (under the crescent).
- `face-annotated.png`: notes on what each part of the face is for.
- `PocketPixel-Regular.ttf`: the font used throughout.

## Updating the watch face after changing the design

From the repository root:

```bash
python3 tools/make_watchface_assets.py
```

This regenerates `watchface/face_background.h`, `watchface/PocketPixel.h`, `watchface/face_layout.h`
and `watchface/face_sun.h`.
The live areas (time, date, steps, sunrise and sunset) and the sun and arc are set at the top of that script. If you move them in
`face.png`, update their positions there too.

To see how the face will look on the watch, without flashing it:

```bash
python3 tools/preview_watchface.py preview.png "09:47" "15 Nov 26" "4213" "07:20" "16:07" 28 --scale 3
```

Both scripts need `pip install pillow fonttools`.

## Font credit and licence

"Pocket Pixel" by stuck-in-suburbia (2011), made with FontStruct
(http://fontstruct.com/fontstructions/show/460205), is licensed under
[Creative Commons Attribution-ShareAlike 3.0](http://creativecommons.org/licenses/by-sa/3.0/).
`PocketPixel-Regular.ttf` and the converted `watchface/PocketPixel.h` are shared under that same licence.
