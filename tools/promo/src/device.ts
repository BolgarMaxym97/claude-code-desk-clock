// Screens of the current firmware (v1.3 layout: label / percent chips on the gauges, bigger mascot, info card,
// clock faces), drawn on a 240×240 canvas like src/screen.ts. Colours and coordinates come from firmware/include/config.h.
const MONO = '"JetBrains Mono", monospace';
const SANS = 'Inter, sans-serif';

const K = {
  BG: '#000', CARD: '#0e0c0a', INK: '#ffe8b0', MUTED: '#706048', DIM: '#584a36', CLAY: '#d84000', AMBER: '#f0a000',
  RED: '#c81818', BLUE: '#6aa6f8', TRACK: '#1c1812', CHIP: '#4c2e0a', GREY: '#f0e4c8', LEAF: ['#d84000', '#c81010', '#702000'],
};
const MASCOT = ['..oooooooooo..', '..ooeooooeoo..', '..ooeooooeoo..', 'oooooooooooooo', 'oooooooooooooo', '..oooooooooo..', '..o.o....o.o..', '..o.o....o.o..'];
const BLINK = ['..oooooooooo..', '..oooooooooo..', '..ooeooooeoo..', 'oooooooooooooo', 'oooooooooooooo', '..oooooooooo..', '..o.o....o.o..', '..o.o....o.o..'];
type Rows = string[];
export const ICON: Record<string, Rows> = {
  sunCloud: ['..y.y.......', '...yyy......', '..yyyyy.....', '.yyyywww....', '..yywwwwww..', '..wwwwwwwww.', '.wwwwwwwwwww', '..wwwwwwwww.', '............', '............'],
  claude: ['.....c......', '..c..c..c...', '...c.c.c....', '....ccc.....', 'ccccccccccc.', '....ccc.....', '...c.c.c....', '..c..c..c...', '.....c......', '............'],
  calendar: ['..i.....i...', '.cciccccicc.', '.cccccccccc.', '.iiiiiiiiii.', '.i.ii.ii.ii.', '.iiiiiiiiii.', '.ii.ii.ii.i.', '.iiiiiiiiii.', '............', '............'],
  msg: ['............', '.ffffffffff.', 'ffffffffffff', 'ff........ff', 'ff.ffffff.ff', 'ff........ff', 'ffffffffffff', '.ffffffffff.', '..ff........', '.f..........'],
};
const PIX = [[14, 17, 19, 21, 25, 17, 14], [4, 12, 4, 4, 4, 4, 14], [14, 17, 1, 2, 4, 8, 31], [31, 2, 4, 2, 1, 17, 14], [2, 6, 10, 18, 31, 2, 2],
  [31, 16, 30, 1, 1, 17, 14], [6, 8, 16, 30, 17, 17, 14], [31, 1, 2, 4, 8, 8, 8], [14, 17, 17, 14, 17, 17, 14], [14, 17, 17, 15, 1, 2, 12]];
const SEG = [0x3f, 0x06, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 0x07, 0x7f, 0x6f];

type G = CanvasRenderingContext2D;
const px = (g: G, x: number, y: number, w: number, h: number, c: string) => { g.fillStyle = c; g.fillRect(x, y, w, h); };
function rr(g: G, x: number, y: number, w: number, h: number, r: number, c: string) { g.fillStyle = c; g.beginPath(); g.roundRect(x, y, w, h, r); g.fill(); }
function chip(g: G, x: number, y: number, w: number, h: number, c: string) { px(g, x + 2, y, w - 4, h, c); px(g, x + 1, y + 1, w - 2, h - 2, c); px(g, x, y + 2, w, h - 4, c); }
function txt(g: G, s: string, x: number, y: number, size: number, c: string, align: CanvasTextAlign = 'left', font = SANS, w = 700) {
  g.font = `${w} ${size}px ${font}`; g.fillStyle = c; g.textAlign = align; g.textBaseline = 'alphabetic'; g.fillText(s, x, y);
  return g.measureText(s).width;
}
function icon(g: G, rows: Rows, x: number, y: number, fg: string) {
  const map: Record<string, string> = { y: K.AMBER, w: K.GREY, b: K.BLUE, f: fg, c: K.CLAY, i: K.INK };
  rows.forEach((r, j) => [...r].forEach((ch, i) => { if (map[ch]) px(g, x + i * 2, y + j * 2, 2, 2, map[ch]); }));
}
function mascot(g: G, x: number, y: number, s: number, blink: boolean) {
  (blink ? BLINK : MASCOT).forEach((r, j) => [...r].forEach((ch, i) => { if (ch !== '.') px(g, x + i * s, y + j * s, s, s, ch === 'e' ? '#000' : K.CLAY); }));
}

export type Frame = { left: string; pill?: string; pillC?: string; work?: boolean; h5?: number; d7?: number };
export function frame(g: G, { left, pill, pillC = K.DIM, work = false, h5 = 46, d7 = 28 }: Frame) {
  px(g, 0, 0, 240, 240, K.BG);
  const w = txt(g, left, 6, 16, 13, K.INK);
  if (pill) {
    g.font = `700 13px ${SANS}`;
    const pw = g.measureText(pill).width + 12, x = 6 + w + 8;
    chip(g, x, 3, pw, 18, pillC);
    txt(g, pill, x + pw / 2, 16, 13, K.INK, 'center');
  }
  g.beginPath(); g.arc(229, 11, 4, 0, 7);
  if (work) { g.fillStyle = K.CLAY; g.fill(); } else { g.strokeStyle = K.INK; g.lineWidth = 2; g.stroke(); }
  for (const [cx, lab, pct] of [[13, '5h', h5], [227, '7d', d7]] as [number, string, number][]) {
    const x = cx + (cx < 120 ? 1 : -1);
    chip(g, x - 14, 26, 28, 20, K.CHIP); txt(g, lab, x, 41, 13, K.INK, 'center', MONO);
    rr(g, cx - 4, 50, 8, 140, 3, K.TRACK);
    const fh = 140 * pct / 100;
    rr(g, cx - 4, 190 - fh, 8, fh, 3, K.CLAY);
    chip(g, x - 14, 194, 28, 20, K.CHIP); txt(g, String(pct), x, 209, 13, K.INK, 'center', MONO);
  }
}

export function stage(g: G, sec: number) {
  const blink = (sec % 3.2) > 3.0;
  const hop = Math.max(0, Math.sin(sec * 5)) * ((sec % 6) < 1.2 ? 6 : 0);
  mascot(g, 120 - 42, 80 + 86 - 12 - 48 - hop, 6, blink);
  for (let i = 0; i < 10; i++) rr(g, 40 + i * 17, 80 + 86 - 6 + (i % 2), 8, 4, 2, K.LEAF[i % 3]);
}

export function card(g: G, ic: Rows, l1: string, l2: string, o: { bg?: string; fg?: string; sub?: string; dots?: [number, number] } = {}) {
  const { bg = K.CARD, fg = K.INK, sub = K.MUTED, dots } = o;
  rr(g, 30, 170, 180, 42, 8, bg);
  icon(g, ic, 38, 180, fg);
  txt(g, l1, 66, 188, 15, fg, 'left', SANS, 600);
  if (l2) txt(g, l2, 66, 203, 12, sub, 'left', SANS, 500);
  if (dots) {
    const [n, on] = dots, w = (n - 1) * 10;
    for (let i = 0; i < n; i++) { g.fillStyle = i === on ? K.INK : K.DIM; g.beginPath(); g.arc(120 - w / 2 + i * 10, 226, i === on ? 3 : 2.5, 0, 7); g.fill(); }
  }
}

// ---- clock faces ----
const timeClassic = (g: G, t: string) => txt(g, t, 120, 72, 58, K.INK, 'center', MONO);
function seg7(g: G, d: number, x: number, y: number) {
  const w = 30, h = 46, t = 6, on = K.AMBER, off = '#1c1408';
  const R = [[x + t, y, w - 2 * t, t], [x + w - t, y + t, t, h / 2 - t], [x + w - t, y + h / 2, t, h / 2 - t], [x + t, y + h - t, w - 2 * t, t],
    [x, y + h / 2, t, h / 2 - t], [x, y + t, t, h / 2 - t], [x + t, y + h / 2 - t / 2, w - 2 * t, t]];
  R.forEach((r, k) => px(g, r[0], r[1], r[2], r[3], (SEG[d] >> k) & 1 ? on : off));
}
export const FACES = ['classic', 'minimal', 'pixel', 'segment', 'flip', 'terminal', 'binary'] as const;
export function face(g: G, f: number, hh: number, mm: number, sec: number) {
  const t = `${String(hh).padStart(2, '0')}:${String(mm).padStart(2, '0')}`, dg = [hh / 10 | 0, hh % 10, mm / 10 | 0, mm % 10];
  if (f === 0) timeClassic(g, t);
  if (f === 1) {
    txt(g, t.slice(0, 2), 111, 66, 44, K.INK, 'right', MONO, 800);
    if (sec % 1 < 0.5) txt(g, ':', 120, 64, 44, K.MUTED, 'center', MONO, 800);
    txt(g, t.slice(3), 129, 66, 44, K.INK, 'left', MONO, 800);
  }
  if (f === 2) {
    let x = 45;
    dg.forEach((d, i) => {
      PIX[d].forEach((bits, r) => { for (let k = 0; k < 5; k++) if (bits & (16 >> k)) px(g, x + k * 6, 32 + r * 6, 5, 5, K.CLAY); });
      x += 30 + (i === 1 ? 0 : 6);
      if (i === 1) { px(g, x + 3, 44, 5, 5, K.CLAY); px(g, x + 3, 56, 5, 5, K.CLAY); x += 12; }
    });
  }
  if (f === 3) {
    let x = 38;
    dg.forEach((d, i) => { seg7(g, d, x, 30); x += 36 + (i === 1 ? 14 : 0); });
    px(g, 117, 42, 6, 6, K.AMBER); px(g, 117, 60, 6, 6, K.AMBER);
  }
  if (f === 4) for (const [cx, s] of [[78, t.slice(0, 2)], [162, t.slice(3)]] as [number, string][]) {
    rr(g, cx - 40, 28, 80, 48, 8, K.TRACK); txt(g, s, cx, 70, 44, K.INK, 'center', MONO, 800); px(g, cx - 40, 51, 80, 2, '#000');
  }
  if (f === 5) {
    txt(g, '$ date +%H:%M', 34, 40, 12, K.MUTED, 'left', MONO, 600);
    const w = txt(g, t, 34, 76, 40, K.INK, 'left', MONO, 800);
    if (sec % 1 < 0.5) px(g, 34 + w + 4, 46, 16, 30, K.CLAY);
  }
  if (f === 6) {
    const bits = [2, 4, 3, 4];
    dg.forEach((v, i) => {
      const cx = 60 + i * 34 + (i > 1 ? 12 : 0);
      for (let b = 0; b < bits[i]; b++) { g.beginPath(); g.fillStyle = (v >> b) & 1 ? K.CLAY : '#2a2018'; g.arc(cx, 66 - b * 11, 4.5, 0, 7); g.fill(); }
    });
    txt(g, t, 120, 78, 11, K.MUTED, 'center', MONO, 600);
  }
}

// ---- whole screens used by the video ----
const WX = ['+3° cloudy', 'feels +1° · rain 18h'] as const;
export function infoScreen(g: G, sec: number) {
  const step = Math.min(2, Math.floor(sec / 1.35));
  frame(g, { left: 'THU 26 NOV', pill: step === 2 ? '8m' : undefined, pillC: K.BLUE });
  timeClassic(g, '10:52');
  stage(g, sec);
  if (step === 0) card(g, ICON.sunCloud, WX[0], WX[1], { dots: [3, 0] });
  if (step === 1) card(g, ICON.claude, 'Today 3:20', '27 answers · 4 commits', { dots: [3, 1] });
  if (step === 2) card(g, ICON.calendar, 'In 8 min', 'Design review', { bg: '#183470', sub: '#c8d8f8' });
}

export function faceScreen(g: G, f: number, sec: number) {
  frame(g, { left: 'THU 26 NOV' });
  face(g, f, 10, 52 + (f > 3 ? 1 : 0), sec);
  stage(g, sec);
  card(g, ICON.sunCloud, WX[0], WX[1], { dots: [3, 0] });
}

export function pinnedScreen(g: G, sec: number, draw: (g: G) => void) { draw(g); }

export function sayScreen(g: G, sec: number, shown: boolean) {
  frame(g, { left: 'THU 26 NOV' });
  timeClassic(g, '10:58');
  stage(g, sec);
  if (shown) card(g, ICON.msg, 'Deploy done', 'api · 4 min 12 s', { bg: '#144c24', sub: '#c0f0c8' });
  else card(g, ICON.sunCloud, WX[0], WX[1], { dots: [3, 0] });
}

export function contextScreen(g: G, sec: number) {
  frame(g, { left: '10:47  THU 26 NOV', work: true });
  mascot(g, 78, 48 - Math.max(0, Math.sin(sec * 6)) * 4, 6, (sec % 2.6) > 2.45);
  txt(g, 'Running', 120, 133, 24, K.CLAY, 'center', SANS, 800);
  txt(g, 'npm test', 120, 159, 15, K.INK, 'center', MONO, 600);
  txt(g, 'my-app · 01:12', 120, 184, 13, K.INK, 'center', SANS, 500);
  const pct = Math.min(93, Math.round(62 + sec * 12)), lv = pct >= 90 ? 2 : pct >= 80 ? 1 : 0, c = lv === 2 ? K.RED : lv === 1 ? K.AMBER : K.MUTED;
  txt(g, (lv === 2 ? 'compact soon ' : 'context ') + pct + '%', 120, 212, 13, c, 'center', SANS, 500);
  px(g, 50, 218, 140, 4, K.TRACK); px(g, 50, 218, 140 * pct / 100, 4, lv ? c : K.CLAY);
}
