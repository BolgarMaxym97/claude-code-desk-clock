// ~27 s promo: a warm, light "desk" look (paper background, editorial serif captions, pixel-block transitions),
// the clock as a 3D model whose screen runs the real scene code.
import { Environment, Lightformer } from '@react-three/drei';
import { useThree } from '@react-three/fiber';
import { ThreeCanvas } from '@remotion/three';
import React from 'react';
import { AbsoluteFill, Audio, Easing, interpolate, Sequence, spring, staticFile, useCurrentFrame, useVideoConfig } from 'remotion';
import * as THREE from 'three';
import { ClockModel, SoftShadow, useScreen } from './Clock3D';
import { C, MONO, SANS, SERIF, SERIF_I } from './fonts';
import { drawBoot, drawCell } from './screen';
import { contextScreen, faceScreen, FACES, infoScreen, sayScreen } from './device';
import { idle, limits, MONTAGE, work } from './shots';

export const FPS = 30;
// scene starts (frames)
export const S = { intro: 0, status: 75, alert: 195, limits: 285, info: 375, anim: 495, faces: 627, phone: 717, say: 822, lang: 897,
  photos: 972, setup: 1050, outro: 1125, end: 1215 };

// ---------- screen content over time ----------
function drawAt(ctx: CanvasRenderingContext2D, f: number) {
  const sec = f / FPS;
  if (f < 42) return drawBoot(ctx, sec);
  if (f < S.status) return drawCell(ctx, idle('pack', 1, sec));
  if (f < S.status + 42) return drawCell(ctx, idle('pack', 1, sec));
  if (f < S.status + 62) return drawCell(ctx, work('think', sec));
  if (f < S.alert) return drawCell(ctx, work('bash', sec));
  if (f < S.alert + 48) return drawCell(ctx, work('permission', sec));
  if (f < S.limits) return drawCell(ctx, work('yourturn', sec));
  if (f < S.limits + 45) return drawCell(ctx, limits((f - S.limits) / FPS));
  if (f < S.info) return contextScreen(ctx, (f - S.limits - 45) / FPS);
  if (f < S.anim) return infoScreen(ctx, (f - S.info) / FPS);
  if (f < S.faces) {
    const i = Math.min(MONTAGE.length - 1, Math.floor((f - S.anim) / 19));
    const [pack, num] = MONTAGE[i];
    return drawCell(ctx, idle(pack, num, (f - S.anim - i * 19) / FPS + 0.5));
  }
  if (f < S.phone) return faceScreen(ctx, Math.min(FACES.length - 1, Math.floor((f - S.faces) / 13)), sec);
  if (f < S.say) return drawCell(ctx, idle('pack', 13, (f - S.phone) / FPS + 0.5));  // the guitar, kept from the phone
  if (f < S.lang) return sayScreen(ctx, sec, f >= S.say + SAY_SENT);
  if (f < S.photos) return drawCell(ctx, work('bash', sec, Math.floor((f - S.lang) / 22) % 2 ? 'uk' : 'en'));
  if (f < S.outro) return drawCell(ctx, work('done', sec));
  return drawCell(ctx, idle('pack', 13, sec));
}

// ---------- camera & clock placement keyframes ----------
type V = [number, number, number];
type K = { f: number; cam: [number, number, number]; look: [number, number, number]; pos: [number, number, number]; rotY: number };
const A = { cam: [-1.5, 1.4, 14] as V, look: [-1.5, 0, 0] as V, pos: [3.4, 0, 0] as V, rotY: -0.42 };
const A2 = { cam: [-1.2, 1.2, 13.5] as V, look: [-1.2, 0, 0] as V, pos: [3.4, 0, 0] as V, rotY: -0.36 };
const CLOSE = { cam: [-1.7, 0.35, 12.2] as V, look: [-1.7, -0.1, 2.6] as V, pos: [0, 0, 0] as V, rotY: 0 };  // whole screen, both gauges
const CLOSE2 = { cam: [-1.6, 0.3, 11.8] as V, look: [-1.6, -0.1, 2.6] as V, pos: [0, 0, 0] as V, rotY: 0 };
const KEYS: K[] = [
  { f: 0, cam: [0, 1.0, 30], look: [0, 0.2, 0], pos: [0, -0.3, 0], rotY: 0.35 },
  { f: 60, cam: [0.2, 1.6, 18.5], look: [0.2, 0.7, 0], pos: [3.6, -0.2, 0], rotY: -0.35 },
  { f: S.status, ...A },
  { f: S.alert - 1, ...A2 },
  { f: S.alert + 14, cam: [4.5, 2.2, 17], look: [1.4, 0.8, 0], pos: [3.4, 0, 0], rotY: 0.2 },
  { f: S.limits - 1, cam: [-1.0, 1.6, 16.5], look: [1.4, 0.8, 0], pos: [3.4, 0, 0], rotY: -0.25 },
  { f: S.limits + 16, ...CLOSE },
  { f: S.info - 1, ...CLOSE2 },
  { f: S.info + 14, ...A },
  { f: S.anim - 1, ...A2 },
  { f: S.anim + 14, cam: [1.5, 2.4, 17.5], look: [1.3, 0.6, 0], pos: [3.2, 0, 0], rotY: 0.3 },
  { f: S.faces - 1, cam: [-1.0, 1.6, 16.5], look: [1.3, 0.6, 0], pos: [3.2, 0, 0], rotY: -0.25 },
  { f: S.faces + 14, ...CLOSE },
  { f: S.phone - 1, ...CLOSE2 },
  { f: S.phone + 14, ...A },
  { f: S.say - 1, ...A2 },
  { f: S.say + 14, ...A },
  { f: S.lang - 1, ...A2 },
  { f: S.lang + 12, cam: [1.0, 0.6, 14.5], look: [1.4, 0.3, 0], pos: [3.6, 0, 0], rotY: -0.1 },
  { f: S.photos, cam: [0.6, 1.4, 16.5], look: [0.6, 0.4, 0], pos: [3.4, 0, 0], rotY: -0.1 },
  { f: S.setup, cam: [-1.4, 1.8, 19], look: [-1.4, 0.2, 0], pos: [2.9, 0, 0], rotY: 5.7332 },
  { f: S.outro - 1, cam: [-1.4, 1.6, 18.4], look: [-1.4, 0.2, 0], pos: [2.9, 0, 0], rotY: 5.8332 },
  { f: S.outro + 20, cam: [0, 3.8, 24], look: [0, 2.9, 0], pos: [0, 0.3, 0], rotY: 6.0832 },
  { f: S.end, cam: [0, 3.6, 23.2], look: [0, 2.9, 0], pos: [0, 0.3, 0], rotY: 6.4332 },
];

function keyAt(f: number): K {
  let i = 0;
  while (i < KEYS.length - 2 && KEYS[i + 1].f <= f) i++;
  const a = KEYS[i], b = KEYS[i + 1];
  const u = Easing.inOut(Easing.cubic)(Math.min(1, Math.max(0, (f - a.f) / (b.f - a.f))));
  const mix = (x: number[], y: number[]) => x.map((v, k) => v + (y[k] - v) * u) as [number, number, number];
  return { f, cam: mix(a.cam, b.cam), look: mix(a.look, b.look), pos: mix(a.pos, b.pos), rotY: a.rotY + (b.rotY - a.rotY) * u };
}

function Rig({ k }: { k: K }) {
  const { camera } = useThree();
  // set synchronously during render: the frame is drawn right after, effects would lag one frame
  camera.position.set(...k.cam);
  camera.lookAt(new THREE.Vector3(...k.look));
  camera.updateMatrixWorld();
  return null;
}

export function Lights() {
  return (
    <>
      <ambientLight intensity={0.55} color="#FFF1DC" />
      <directionalLight position={[6, 9, 8]} intensity={2.1} color="#FFE7C2" castShadow />
      <directionalLight position={[-8, 3, -6]} intensity={0.9} color="#FFD2A8" />
      <Environment resolution={256} frames={1}>
        <Lightformer form="rect" intensity={2.2} color="#FFF4E2" position={[0, 6, 6]} scale={[12, 4, 1]} />
        <Lightformer form="rect" intensity={1.2} color="#FFD9B0" position={[-7, 2, 2]} rotation-y={Math.PI / 2} scale={[8, 4, 1]} />
        <Lightformer form="ring" intensity={0.8} color="#FFFFFF" position={[6, 3, 4]} scale={2} />
      </Environment>
    </>
  );
}

function Scene3D() {
  const f = useCurrentFrame();
  const k = keyAt(f);
  const screen = useScreen((ctx) => drawAt(ctx, f));
  const float = Math.sin(f / 40) * 0.05;
  const glow = interpolate(f, [0, 14, 30], [0.05, 0.05, 1], { extrapolateRight: 'clamp' });
  return (
    <>
      <Rig k={k} />
      <Lights />
      <group position={[k.pos[0], k.pos[1] + float, k.pos[2]]} rotation={[0, k.rotY, 0]}>
        <ClockModel screen={screen} glow={glow} />
        <SoftShadow y={-2.52 - float} />
      </group>
    </>
  );
}

// ---------- 2D overlays ----------
function Caption({ kicker, text, at, until, x = 120, y = 104, width = 900 }: { kicker: string; text: string; at: number; until: number; x?: number; y?: number; width?: number }) {
  const f = useCurrentFrame();
  const { fps } = useVideoConfig();
  if (f < at || f >= until) return null;
  const out = interpolate(f, [until - 10, until], [1, 0], { extrapolateLeft: 'clamp' });
  const words = text.split(' ');
  return (
    <div style={{ position: 'absolute', left: x, top: y, width, opacity: out }}>
      <div style={{ fontFamily: MONO, fontSize: 22, fontWeight: 700, letterSpacing: 4, color: C.clay, display: 'flex', alignItems: 'center', gap: 14,
        opacity: interpolate(f - at, [0, 8], [0, 1], { extrapolateRight: 'clamp' }) }}>
        <PixelMark size={4} />{kicker}
      </div>
      <div style={{ marginTop: 18, fontFamily: SERIF, fontWeight: 600, fontSize: 76, lineHeight: 1.08, color: C.ink, letterSpacing: -1.5 }}>
        {words.map((w, i) => {
          const s = spring({ frame: f - at - 4 - i * 3, fps, config: { damping: 18, mass: 0.6 } });
          return <span key={i} style={{ display: 'inline-block', marginRight: 18, opacity: s, transform: `translateY(${(1 - s) * 34}px)` }}>{w}</span>;
        })}
      </div>
    </div>
  );
}

// tiny mascot made of divs (14×8 sprite)
const SPRITE = ['..oooooooooo..', '..ooeooooeoo..', '..ooeooooeoo..', 'oooooooooooooo', 'oooooooooooooo', '..oooooooooo..', '..o.o....o.o..', '..o.o....o.o..'];
export function PixelMark({ size = 4, color = C.clay }: { size?: number; color?: string }) {
  return (
    <div style={{ display: 'grid', gridTemplateColumns: `repeat(14, ${size}px)`, gridAutoRows: size }}>
      {SPRITE.join('').split('').map((ch, i) => <div key={i} style={{ background: ch === 'o' ? color : ch === 'e' ? C.ink : 'transparent' }} />)}
    </div>
  );
}

// pixel-block wipe centred on frame `at`
function PixelWipe({ at }: { at: number }) {
  const f = useCurrentFrame();
  const d = f - at;
  if (d < -12 || d > 12) return null;
  const COLS = 24, ROWS = 14, cw = 1920 / COLS, ch = 1080 / ROWS;
  const blocks = [];
  for (let r = 0; r < ROWS; r++)
    for (let c = 0; c < COLS; c++) {
      const order = (c + r * 0.6) / (COLS + ROWS * 0.6);  // diagonal sweep
      const inS = interpolate(d, [-12 + order * 9, -4 + order * 9], [0, 1], { extrapolateLeft: 'clamp', extrapolateRight: 'clamp' });
      const outS = interpolate(d, [1 + order * 9, 6 + order * 9], [1, 0], { extrapolateLeft: 'clamp', extrapolateRight: 'clamp' });
      const s = Math.min(inS, outS);
      if (s <= 0) continue;
      blocks.push(<div key={r * COLS + c} style={{ position: 'absolute', left: c * cw, top: r * ch, width: cw + 1, height: ch + 1,
        background: (r + c) % 7 === 0 ? C.orange : C.clay, transform: `scale(${s})` }} />);
    }
  return <AbsoluteFill>{blocks}</AbsoluteFill>;
}

function Terminal({ from }: { from: number }) {
  const f = useCurrentFrame() - from;
  if (f < 0 || f > S.alert - S.status) return null;
  const prompt = 'add tests for the parser';
  const typed = prompt.slice(0, Math.max(0, Math.floor((f - 8) / 1.2)));
  const show = (at: number) => ({ opacity: interpolate(f, [at, at + 6], [0, 1], { extrapolateLeft: 'clamp', extrapolateRight: 'clamp' }) });
  const enter = spring({ frame: f, fps: FPS, config: { damping: 20 } });
  const out = interpolate(f, [S.alert - S.status - 10, S.alert - S.status], [1, 0], { extrapolateLeft: 'clamp' });
  return (
    <div style={{ position: 'absolute', left: 120, top: 370, width: 780, height: 430, borderRadius: 20, background: '#211E1A',
      boxShadow: '0 30px 60px rgba(70,45,20,0.22), 0 4px 14px rgba(70,45,20,0.14)', opacity: enter * out,
      transform: `translateY(${(1 - enter) * 60}px)`, fontFamily: MONO, fontSize: 27, color: '#EDE6DA', overflow: 'hidden' }}>
      <div style={{ height: 48, display: 'flex', alignItems: 'center', gap: 10, padding: '0 20px', background: '#2C2823' }}>
        {['#E5655A', '#E8B04A', '#6BBF6A'].map((c) => <div key={c} style={{ width: 14, height: 14, borderRadius: 7, background: c }} />)}
        <span style={{ marginLeft: 16, fontSize: 18, color: '#9A9184' }}>~/my-app — claude</span>
      </div>
      <div style={{ padding: '26px 30px', lineHeight: 1.7 }}>
        <div><span style={{ color: C.clay }}>{'>'}</span> {typed}<span style={{ opacity: f % 16 < 8 ? 1 : 0 }}>▍</span></div>
        <div style={show(38)}><span style={{ color: C.clay }}>✻</span> <span style={{ color: '#9A9184' }}>Thinking…</span></div>
        <div style={show(58)}><span style={{ color: '#6BBF6A' }}>●</span> Bash(<span style={{ color: '#E8B04A' }}>npm test</span>)</div>
        <div style={{ ...show(84), color: '#9A9184' }}>  ⎿ 42 passed, 0 failed</div>
      </div>
    </div>
  );
}

const SAY_SENT = 40;  // frame (from S.say) when the command runs and the card appears

function SayTerminal() {
  const f = useCurrentFrame() - S.say;
  const len = S.lang - S.say;
  if (f < 0 || f >= len) return null;
  const cmd = 'claude-clock say "Deploy done" "api · 4 min 12 s"';
  const typed = cmd.slice(0, Math.max(0, Math.floor((f - 4) * 1.6)));
  const enter = spring({ frame: f, fps: FPS, config: { damping: 20 } });
  const out = interpolate(f, [len - 10, len], [1, 0], { extrapolateLeft: 'clamp' });
  return (
    <div style={{ position: 'absolute', left: 120, top: 420, width: 860, height: 230, borderRadius: 20, background: '#211E1A',
      boxShadow: '0 30px 60px rgba(70,45,20,0.22), 0 4px 14px rgba(70,45,20,0.14)', opacity: enter * out,
      transform: `translateY(${(1 - enter) * 60}px)`, fontFamily: MONO, fontSize: 25, color: '#EDE6DA', overflow: 'hidden' }}>
      <div style={{ height: 44, display: 'flex', alignItems: 'center', gap: 10, padding: '0 20px', background: '#2C2823' }}>
        {['#E5655A', '#E8B04A', '#6BBF6A'].map((c) => <div key={c} style={{ width: 13, height: 13, borderRadius: 7, background: c }} />)}
        <span style={{ marginLeft: 16, fontSize: 17, color: '#9A9184' }}>~/api — deploy.sh</span>
      </div>
      <div style={{ padding: '24px 28px', lineHeight: 1.7 }}>
        <div style={{ color: '#9A9184' }}>$ ./deploy.sh &amp;&amp; {'\\'}</div>
        <div><span style={{ color: C.clay }}>$</span> {typed}<span style={{ opacity: f % 16 < 8 && f < SAY_SENT ? 1 : 0 }}>▍</span></div>
        <div style={{ color: '#6BBF6A', opacity: interpolate(f, [SAY_SENT, SAY_SENT + 6], [0, 1], { extrapolateLeft: 'clamp', extrapolateRight: 'clamp' }) }}>✓ on the clock</div>
      </div>
    </div>
  );
}

// The settings page of the clock on a phone: favourites with stars, the one that plays now.
function PhoneSettings() {
  const f = useCurrentFrame() - S.phone;
  const len = S.say - S.phone;
  if (f < 0 || f >= len) return null;
  const enter = spring({ frame: f, fps: FPS, config: { damping: 18 } });
  const out = interpolate(f, [len - 10, len], [1, 0], { extrapolateLeft: 'clamp' });
  const rows: [string, string, number][] = [['Coffee', 'daily', 18], ['Guitar', 'daily', 30], ['Rocket', 'daily', 999], ['Chess', 'weekend', 42],
    ['Dragon', 'rare', 54], ['Fishing', 'daily', 999]];
  const D = { bg: '#141210', card: '#1E1A16', line: '#3A322A', ink: '#FFE8B0', muted: '#A8987C', clay: '#D84000', amber: '#F0A000' };
  const favs = rows.filter((r) => f >= r[2]).length;
  return (
    <div style={{ position: 'absolute', left: 150, top: 350, width: 400, height: 860, borderRadius: 52, background: '#0B0A09', padding: 14,
      boxShadow: '0 40px 80px rgba(70,45,20,0.28)', opacity: enter * out, transformOrigin: 'top left',
      transform: `translateY(${(1 - enter) * 80}px) rotate(-2deg) scale(0.76)` }}>
      <div style={{ width: '100%', height: '100%', borderRadius: 40, background: D.bg, overflow: 'hidden', fontFamily: SANS, color: D.ink, padding: '44px 22px' }}>
        <div style={{ fontWeight: 700, fontSize: 24, display: 'flex', justifyContent: 'space-between' }}>Claude Clock<span style={{ fontSize: 14, color: D.muted }}>EN · УКР</span></div>
        <div style={{ marginTop: 22, fontSize: 14, letterSpacing: 1.2, color: D.muted, fontWeight: 600, display: 'flex', justifyContent: 'space-between' }}>
          ANIMATION<span style={{ color: D.amber, letterSpacing: 0 }}>{f >= 66 ? 'kept: Guitar · 2 h' : ''}</span></div>
        <div style={{ marginTop: 10, background: D.card, border: `1px solid ${D.line}`, borderRadius: 14, padding: 14, display: 'flex', flexDirection: 'column', gap: 10 }}>
          <div style={{ display: 'flex', gap: 8 }}>
            {['Random', `Favourites (${favs})`].map((t, i) => (
              <div key={t} style={{ padding: '8px 12px', borderRadius: 10, fontSize: 15, border: `1px solid ${i === 1 ? D.clay : D.line}`, color: i === 1 ? D.ink : D.muted }}>{t}</div>))}
          </div>
          <div style={{ display: 'flex', gap: 6, flexWrap: 'wrap' }}>
            {['Every day 34', 'Weekend 4', 'Rare 7', 'Holidays 20', 'Events 23'].map((t, i) => (
              <div key={t} style={{ padding: '6px 9px', borderRadius: 9, fontSize: 13, border: `1px solid ${i < 4 ? D.clay : D.line}`, color: i < 4 ? D.ink : D.muted }}>{t}</div>))}
          </div>
          <div style={{ border: `1px solid ${D.line}`, borderRadius: 10, overflow: 'hidden' }}>
            {rows.map(([n, k, at], i) => {
              const on = f >= at, playing = n === 'Guitar' && f >= 66;
              const pop = on ? spring({ frame: f - at, fps: FPS, config: { damping: 9 } }) : 0;
              return (
                <div key={n} style={{ display: 'flex', alignItems: 'center', gap: 12, padding: '12px 12px', fontSize: 17, borderTop: i ? '1px solid #2A241E' : 'none',
                  background: playing ? '#24180A' : 'transparent' }}>
                  <span style={{ color: on ? D.amber : '#584A36', fontSize: 20, transform: `scale(${1 + pop * 0.3 - (on ? 0.3 : 0)})` }}>{on ? '★' : '☆'}</span>
                  <span>{n}</span>
                  <span style={{ marginLeft: 'auto', fontSize: 13, color: D.muted }}>{playing ? 'playing' : k}</span>
                  <span style={{ color: D.clay, fontWeight: 700 }}>▶</span>
                </div>);
            })}
          </div>
          <div style={{ marginTop: 4, padding: '12px', borderRadius: 10, background: D.clay, color: '#fff', fontWeight: 600, textAlign: 'center', fontSize: 16 }}>Keep playing: Guitar</div>
          <div style={{ padding: '11px', borderRadius: 10, border: `1px solid ${D.line}`, color: D.ink, textAlign: 'center', fontSize: 15 }}>Reset animation settings</div>
        </div>
        <div style={{ marginTop: 18, fontSize: 14, letterSpacing: 1.2, color: D.muted, fontWeight: 600 }}>SCREEN</div>
        <div style={{ marginTop: 10, display: 'flex', gap: 6, flexWrap: 'wrap' }}>
          {['Classic', 'Minimal', 'Pixel', 'Segment', 'Flip', 'Terminal', 'Binary'].map((t, i) => (
            <div key={t} style={{ padding: '6px 9px', borderRadius: 9, fontSize: 13, border: `1px solid ${i === 0 ? D.clay : D.line}`, color: i === 0 ? D.ink : D.muted }}>{t}</div>))}
        </div>
      </div>
    </div>
  );
}

function Specs() {
  const f = useCurrentFrame() - S.photos;
  if (f < 0 || f >= S.setup - S.photos) return null;
  const chips = ['ESP8266 · 2.4 GHz Wi-Fi', '1.54″ 240×240 IPS screen', 'USB-C power, 5 cm cube', 'about $8 on AliExpress'];
  const out = interpolate(f, [S.setup - S.photos - 10, S.setup - S.photos], [1, 0], { extrapolateLeft: 'clamp' });
  return (
    <div style={{ position: 'absolute', left: 120, top: 420, display: 'flex', flexDirection: 'column', gap: 18, opacity: out }}>
      {chips.map((t, i) => {
        const p = spring({ frame: f - 12 - i * 6, fps: FPS, config: { damping: 16 } });
        return (
          <div key={t} style={{ display: 'flex', alignItems: 'center', gap: 18, opacity: p, transform: `translateX(${(1 - p) * -60}px)` }}>
            <div style={{ width: 14, height: 14, background: C.clay }} />
            <span style={{ fontFamily: SANS, fontWeight: 600, fontSize: 34, color: C.ink }}>{t}</span>
          </div>
        );
      })}
    </div>
  );
}

function Steps() {
  const f = useCurrentFrame() - S.setup;
  if (f < 0 || f >= S.outro - S.setup) return null;
  const steps = [['1', 'Upload the firmware', 'from the clock’s own web page'], ['2', 'Scan the QR code', 'and pick your Wi-Fi on the phone'],
    ['3', 'Pair your computer', 'one command · macOS · Linux · Windows']];
  const out = interpolate(f, [S.outro - S.setup - 10, S.outro - S.setup], [1, 0], { extrapolateLeft: 'clamp' });
  return (
    <div style={{ position: 'absolute', left: 120, top: 390, display: 'flex', flexDirection: 'column', gap: 26, opacity: out }}>
      {steps.map(([n, t, s], i) => {
        const p = spring({ frame: f - 10 - i * 8, fps: FPS, config: { damping: 16 } });
        return (
          <div key={n} style={{ display: 'flex', alignItems: 'center', gap: 28, width: 780, padding: '24px 30px', borderRadius: 18, background: C.card,
            boxShadow: '0 14px 34px rgba(70,45,20,0.12)', opacity: p, transform: `translateX(${(1 - p) * -80}px)` }}>
            <div style={{ width: 64, height: 64, borderRadius: 14, background: C.clay, color: '#fff', fontFamily: MONO, fontWeight: 800, fontSize: 32,
              display: 'flex', alignItems: 'center', justifyContent: 'center' }}>{n}</div>
            <div>
              <div style={{ fontFamily: SANS, fontWeight: 700, fontSize: 36, color: C.ink }}>{t}</div>
              <div style={{ fontFamily: SANS, fontWeight: 500, fontSize: 24, color: C.muted, marginTop: 4 }}>{s}</div>
            </div>
          </div>
        );
      })}
    </div>
  );
}

function Outro() {
  const f = useCurrentFrame() - S.outro;
  if (f < 0) return null;
  const p = (d: number) => spring({ frame: f - d, fps: FPS, config: { damping: 18 } });
  return (
    <div style={{ position: 'absolute', left: 0, right: 0, top: 120, textAlign: 'center' }}>
      <div style={{ display: 'flex', justifyContent: 'center', opacity: p(16) }}><PixelMark size={7} /></div>
      <div style={{ marginTop: 26, fontFamily: SERIF, fontWeight: 600, fontSize: 104, letterSpacing: -2, color: C.ink, opacity: p(20),
        transform: `translateY(${(1 - p(20)) * 30}px)` }}>Claude Code Desk Clock</div>
      <div style={{ marginTop: 14, fontFamily: MONO, fontWeight: 600, fontSize: 30, color: C.clay, opacity: p(28) }}>github.com/BolgarMaxym97/claude-code-desk-clock</div>
      <div style={{ marginTop: 16, fontFamily: SANS, fontWeight: 500, fontSize: 21, color: C.muted, opacity: p(36) }}>
        Free &amp; open source (MIT) · Unofficial fan project, not affiliated with Anthropic
      </div>
    </div>
  );
}

function Grain() {  // soft paper vignette
  return <AbsoluteFill style={{ pointerEvents: 'none', background: 'radial-gradient(ellipse at 50% 40%, rgba(255,255,255,0) 55%, rgba(120,90,50,0.16) 100%)' }} />;
}

export const Promo: React.FC = () => {
  const { width, height } = useVideoConfig();
  const f = useCurrentFrame();
  return (
    <AbsoluteFill style={{ background: `linear-gradient(180deg, ${C.paper} 0%, ${C.paper2} 100%)` }}>
      {(
        <ThreeCanvas width={width} height={height} camera={{ fov: 30, position: [0, 2, 18] }} gl={{ antialias: true, alpha: true }} shadows>
          <Scene3D />
        </ThreeCanvas>
      )}
      <Terminal from={S.status} />
      <PhoneSettings />
      <SayTerminal />
      <Specs />
      <Steps />
      <Outro />
      <Caption kicker="CLAUDE CODE DESK CLOCK" text="A tiny desk clock for Claude Code." at={24} until={S.status - 2} />
      <Caption kicker="LIVE STATUS" text="See what Claude is doing." at={S.status + 6} until={S.alert} />
      <Caption kicker="HEADS UP" text="Know when it needs you." at={S.alert + 8} until={S.limits} />
      <Caption kicker="USAGE LIMITS" text="Limits, time to reset and context." at={S.limits + 10} until={S.info} width={640} />
      <Caption kicker="INFO LINE" text="Weather, your next meeting, your day." at={S.info + 10} until={S.anim} width={700} />
      <Caption kicker="BETWEEN TASKS" text="90+ little pixel animations." at={S.anim + 10} until={S.faces} width={760} />
      <Caption kicker="CLOCK FACES" text="Seven clock faces." at={S.faces + 10} until={S.phone} width={640} />
      <Caption kicker="FROM YOUR PHONE" text="Pick your favourite animations." at={S.phone + 8} until={S.say} width={760} />
      <Caption kicker="FROM YOUR SCRIPTS" text="Send it a message." at={S.say + 6} until={S.lang} width={760} />
      <Caption kicker="TWO LANGUAGES" text="English or українська." at={S.lang + 8} until={S.photos} width={680} />
      <Caption kicker="THE HARDWARE" text="Runs on a cheap ESP8266 desk clock." at={S.photos + 6} until={S.setup} width={760} />
      <Caption kicker="SETUP IN 5 MINUTES" text="No soldering. No compiling." at={S.setup + 4} until={S.outro} />
      {[S.status, S.info, S.anim, S.phone, S.photos, S.setup, S.outro].map((t) => <PixelWipe key={t} at={t} />)}
      <Grain />
      <Sequence from={0}><Audio src={staticFile('music.wav')} volume={(fr) => interpolate(fr, [0, 10, S.end - 40, S.end], [0, 0.8, 0.8, 0], { extrapolateRight: 'clamp' })} /></Sequence>
    </AbsoluteFill>
  );
};
