# Promo video

A ~27 s 1920×1080 promo made with [Remotion](https://www.remotion.dev) and React Three Fiber: a 3D model of the
clock on a warm paper background. Its screen runs the same scene code as the design artifact
(`design/idle-anim/src`, local only), so the animations match the device.

```sh
cd tools/promo
npm install
npm run prep      # src/gen/scenes.js from the design sources + public/music.wav (original lo-fi track, scripts/music.py)
npm run studio    # preview in the browser
npm run render    # -> out/promo.mp4
```

- `src/Promo.tsx`: scenes, camera keyframes, captions, transitions. `src/Clock3D.tsx`: the procedural clock model.
  `src/screen.ts` draws a 240×240 screen; `src/shots.ts` holds the English / Ukrainian texts.
- `npx remotion still src/index.ts Cover out/cover.png --gl=angle` renders the 1280×640 repository cover
  (`docs/media/cover.png`, also usable as GitHub's social preview).
- `out/` is not committed. The video goes to the README (drag it into GitHub's editor) or a release.
