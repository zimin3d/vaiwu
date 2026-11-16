# Art generation prompts

Ready-to-use prompts for regenerating the character/monster art with an
external image model (Stable Diffusion, DALL-E, Kandinsky, Midjourney...).
Each character = one BASE prompt + one emotion modifier from the table.
Keep the STYLE ANCHOR identical across everything so the set looks coherent.

After generating, drop the PNGs into a folder and they can be converted to
firmware format (96x128 RGB565A8) - see tools/gen_sprites.py.

## Style anchor (append to every prompt)

```
full body character sprite, front view, standing pose, centered composition,
flat solid bright magenta background (#FF00FF) with no gradient and no shadow on it,
detailed anime illustration, soft cel shading,
large expressive eyes, pastel color palette, clean lineart, high quality
```

Negative prompt (if the generator supports it):

```
text, watermark, logo, scenery background, multiple characters, cropped body,
extra limbs, blurry, photo, 3d render
```

## Target sizes

Generate large (512-1024 px on the long side) and downscale on conversion;
the firmware needs exactly:

| asset                     | final size | aspect |
|---------------------------|------------|--------|
| character sprites         | 96 x 128   | 3:4    |
| battle monsters           | 96 x 96    | 1:1    |
| home interior / meadow bg | 320 x 140  | 16:7   |

Tips:
- Aspect ratio 3:4 (or 2:3) for characters, 1:1 for monsters - see the size
  table above.
- Generate on a flat magenta background (#FF00FF). The slicer cuts that colour
  out as a chroma key (`tools/art/slice_art.py`), so the sprites get a real
  transparent background.
- One emotion per image: swap the emotion tag, keep everything else fixed.
- `dead` is meant as a comedic "knocked out" state, not gore.

## Emotion modifiers

| emotion | modifier |
|---------|----------|
| idle    | `gentle calm smile, relaxed pose, arms at sides` |
| happy   | `joyful laughing, closed ^ ^ eyes, open mouth smile, small sparkles around` |
| sad     | `sad drooping eyebrows, watery teary eyes, single tear drop, small frown` |
| angry   | `angry furrowed brows, pouting frown, red anger vein mark on head` |
| sleepy  | `sleepy closed eyes, small yawn, "Zzz" letters floating above head` |
| sick    | `dizzy spiral eyes, pale sickly greenish face, big sweat drop on temple` |
| dead    | `knocked out, X X eyes, whole character in grey monochrome, flat mouth` |

---

## Aoi (aoi)

```
adult anime woman, 30+ years old, mature face, long straight pastel blue hair reaching the waist with a
shiny highlight band, jagged fringe, light blue eyes, soft blush, light blue
sundress with thin shoulder straps, fitted bodice and flared skirt with a
darker blue hem band, white socks, dark mary-jane shoes, <EMOTION>
```

## Hina (hina)

```
adult anime woman, 30+ years old, mature face, warm peachy-orange high twintails tied with small cream
bows, jagged fringe, amber eyes, soft blush, oversized cream hoodie with a
kangaroo pocket, drawstrings and ribbed cuffs and hem, white socks, dark
mary-jane shoes, <EMOTION>
```

## Rin (rin)

```
adult anime woman, 30+ years old, mature face, pastel pink bob haircut curled inward with a small white
triangular hairpin, pink eyes, soft blush, japanese sailor school uniform:
white-pink blouse with a dark purple sailor collar with white stripes, pink
neckerchief with a knot, pleated purple skirt, white socks, dark mary-jane
shoes, <EMOTION>
```

## Yuki (yuki)

```
adult anime woman, 30+ years old, mature face, long lavender hime-cut hair with blunt straight bangs,
blunt side locks and a small white flower hair clip, violet eyes, soft blush,
pale lavender kimono with wide hanging sleeves, wrapped collar and a purple
obi belt with a front knot, white socks, dark shoes, <EMOTION>
```

## Akane (akane)

```
adult anime fox woman, 30+ years old, mature face, caramel orange hair with an ahoge, large fox ears with
dark tips and cream fluff inside, big fluffy fox tail with a cream tip,
amber eyes, soft blush, light orange hoodie with a paw print on the kangaroo
pocket and drawstrings, white socks, dark shoes, <EMOTION>
```

## Unicorn (unicorn)

```
adult anime unicorn woman, 30+ years old, mature face, long flowing wavy lilac mane with lighter pastel
streaks, golden spiral horn on the forehead with a sparkle, violet eyes,
soft blush, pale lilac sundress with shoulder straps and a flared skirt,
white socks, dark shoes, <EMOTION>
```

## Kolobok (kolobok) - not humanoid

```
cute round golden bread bun creature (russian kolobok fairy tale), shiny
baked crust with a glossy highlight and small crust cracks, stubby little
feet, simple kawaii face with big eyes and rosy cheeks, <EMOTION>
```

---

## Monsters (1:1, game enemy sprite, flat magenta #FF00FF background)

- **slime**: `cute green slime monster, gooey translucent dome with a lighter
  inner core and trapped bubbles, big glossy highlight, happy-evil grin, kawaii`
- **bat**: `cute purple bat monster, round body, spread membrane wings with
  visible bone struts, big glowing yellow eyes, small white fangs, kawaii`
- **shroom**: `grumpy mushroom monster, red cap with cream spots and a rim
  light, beige stem body, angry eyebrows and a small frown, kawaii`
- **fox**: `sly orange fox head monster, big ears with dark tips, cream muzzle,
  half-lidded amber eyes, fluffy curled tail with cream tip, kawaii`
- **ghost**: `cute white sheet ghost, wavy hem, floating wisps, hollow dark
  eyes and a wailing mouth, faint inner glow, kawaii`
- **golem**: `small stone golem monster, blocky grey rock body and head with
  bevelled edges, glowing yellow eyes, cracks and green moss patches, kawaii`
