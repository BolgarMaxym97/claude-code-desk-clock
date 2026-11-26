// Draws one 240×240 clock screen (a "cell" from the design artifact) onto a canvas, same layout as the
// artifact's HTML board / the firmware. Coordinates are device pixels; the caller scales the context.
import * as scenes from './gen/scenes.js';

export type Cell = Record<string, any>;
export type Pack = 'pack' | 'pack2' | 'pack3' | 'work' | 'reset';

const MONO = '"JetBrains Mono", monospace';
const SANS = 'Inter, sans-serif';
const STAGE = [28, 80, 184, 70];

// artifact tick (120 ms) from video seconds
export const tickAt = (sec: number) => Math.floor(sec * 1000 / 120);

export function cell(pack: Pack, num: number, t: number, over: Cell = {}): Cell {
  const all: Cell[] = (scenes as any)[pack](t);
  const c = all.find((x) => x.num === num);
  if (!c) throw new Error(`no scene ${pack} #${num}`);
  const r = { ...c, ...over };
  if (over.frame || over.body) r.px = c._px(r.frame, r.body);
  return r;
}

function rr(ctx: CanvasRenderingContext2D, x: number, y: number, w: number, h: number, r: number) {
  ctx.beginPath();
  ctx.roundRect(x, y, w, h, Math.min(r, w / 2, h / 2));
  ctx.fill();
}

function txt(ctx: CanvasRenderingContext2D, s: string, x: number, y: number, font: string, color: string,
  align: CanvasTextAlign = 'center', base: CanvasTextBaseline = 'top', spacing = '0px') {
  ctx.font = font;
  ctx.fillStyle = color;
  ctx.textAlign = align;
  ctx.textBaseline = base;
  (ctx as any).letterSpacing = spacing;
  ctx.fillText(s, x, y);
  (ctx as any).letterSpacing = '0px';
}

function rects(ctx: CanvasRenderingContext2D, list: Cell[]) {
  for (const d of list) {
    ctx.globalAlpha = d.o ?? 1;
    ctx.fillStyle = d.c;
    if (d.r) rr(ctx, d.x, d.y, d.w, d.h, d.r); else ctx.fillRect(d.x, d.y, d.w, d.h);
  }
  ctx.globalAlpha = 1;
}

function gauge(ctx: CanvasRenderingContext2D, x0: number, c: Cell, right: boolean) {
  const cx = x0 + 13;
  const label = right ? '7d' : '5h', lc = right ? '#A8A399' : c.g5c;
  const pct = right ? c.g7 : c.g5, fill = right ? '#D97757' : c.g5f;
  const pt = right ? c.g7t : c.g5t, pc = right ? c.g7pc : c.g5pc;
  const sub = right ? c.sub7On : c.subOn, s1 = right ? c.g7s1 : c.g5s1, s2 = right ? c.g7s2 : c.g5s2;
  const pace = right ? c.pace7On : c.paceOn, pp = right ? c.p7 : c.p5;
  const anySub = c.subOn || c.sub7On;  // both columns shorten together (flex rows share the layout)
  txt(ctx, label, cx, 15, `700 13px ${MONO}`, lc, 'center', 'middle');
  const top = 29, bottom = 240 - 8 - 17 - 6 - (anySub ? 28 : 0) - (c.tkMb || 0);
  ctx.fillStyle = '#2A2825';
  rr(ctx, cx - 3.5, top, 7, bottom - top, 4);
  const h = (bottom - top) * pct / 100;
  ctx.fillStyle = fill;
  rr(ctx, cx - 3.5, bottom - h, 7, h, 4);
  if (pace) { ctx.fillStyle = '#F4F1EA'; rr(ctx, cx - 7.5, bottom - (bottom - top) * pp / 100 - 1, 15, 2, 1); }
  txt(ctx, pt, cx, bottom + 6 + 9, `800 14px ${MONO}`, pc, 'center', 'middle');
  if (sub) {
    txt(ctx, s1 || '', cx, bottom + 23 + 7, `700 12px ${MONO}`, '#8A8478', 'center', 'middle');
    txt(ctx, s2 || '', cx, bottom + 37 + 7, `700 12px ${MONO}`, '#8A8478', 'center', 'middle');
  }
}

export function drawCell(ctx: CanvasRenderingContext2D, c: Cell) {
  ctx.fillStyle = c.bg;
  ctx.fillRect(0, 0, 240, 240);
  if (c.gauges) {
    gauge(ctx, 0, c, false);
    gauge(ctx, 214, c, true);
    txt(ctx, String(c.topL).toUpperCase(), 28, 15, `700 13px ${SANS}`, '#A8A399', 'left', 'middle', '0.26px');
    ctx.font = `700 13px ${SANS}`;
    const w = ctx.measureText(c.topR ?? '1').width;
    txt(ctx, c.topR ?? '1', 212, 15, `700 13px ${SANS}`, '#A8A399', 'right', 'middle');
    ctx.fillStyle = c.dotC;
    ctx.beginPath(); ctx.arc(212 - w - 9, 15, 4, 0, Math.PI * 2); ctx.fill();
  }
  if (c.timeOn) txt(ctx, c.time, 120, c.timeTop + 63, `700 60px ${MONO}`, c.timeC, 'center', 'alphabetic', '-3px');  // ink ≈ 31..75 like the device
  ctx.save();
  if (c.clip) { ctx.beginPath(); ctx.rect(STAGE[0], STAGE[1], STAGE[2], STAGE[3]); ctx.clip(); }
  rects(ctx, c.dots);
  c.px.forEach((p: Cell, i: number) => {
    if (p.c === 'transparent') return;
    ctx.fillStyle = p.c;
    ctx.fillRect(c.x + (i % 14) * c.ps, c.y + Math.floor(i / 14) * c.ps, c.ps, c.ps);
  });
  rects(ctx, c.over);
  ctx.restore();
  if (c.verb) {
    txt(ctx, c.verb, 120, 112, `700 24px ${SANS}`, c.vc);
    txt(ctx, c.target, 120, 148, `600 15px ${MONO}`, '#F4F1EA');
    txt(ctx, c.detail, 120, 174, `600 13px ${SANS}`, '#6E6A62');
  }
  if (c.cardOn) {
    ctx.fillStyle = '#1A1816';
    rr(ctx, 30, 152, 180, 34, 7);
    txt(ctx, c.cardL, 40, 169, `600 14px ${SANS}`, c.cardC, 'left', 'middle');
    if (c.cardR) txt(ctx, c.cardR, 200, 169, `600 14px ${MONO}`, '#A8A399', 'right', 'middle');
  }
  if (c.banner) txt(ctx, c.banner, 120, c.bannerY, `800 ${c.bannerS}px ${MONO}`, c.bannerC);
  if (c.frameRing) {  // permission: blinking amber border like the firmware
    ctx.strokeStyle = '#F0B429';
    ctx.lineWidth = 3;
    ctx.strokeRect(1.5, 1.5, 237, 237);
  }
}

// Boot screen (firmware drawBoot): title, Wi-Fi + time steps, version.
export function drawBoot(ctx: CanvasRenderingContext2D, sec: number, lang: 'en' | 'uk' = 'en') {
  ctx.fillStyle = '#0B0B0A';
  ctx.fillRect(0, 0, 240, 240);
  txt(ctx, 'CLAUDE CLOCK', 120, 20, `700 13px ${SANS}`, '#8A8478', 'center', 'alphabetic', '0.5px');
  const wifiOk = sec > 0.9, timeOk = sec > 1.6;
  const step = (y: number, label: string, ok: boolean, active: boolean) => {
    txt(ctx, label, 58, y, `600 15px ${SANS}`, '#F4F1EA', 'left', 'middle');
    if (ok) {
      ctx.fillStyle = '#5FB97A'; ctx.beginPath(); ctx.arc(44, y, 7, 0, Math.PI * 2); ctx.fill();
      ctx.strokeStyle = '#0B0B0A'; ctx.lineWidth = 2; ctx.beginPath(); ctx.moveTo(40, y); ctx.lineTo(43, y + 3); ctx.lineTo(48, y - 3); ctx.stroke();
    } else {
      ctx.strokeStyle = '#4A453E'; ctx.lineWidth = 2; ctx.beginPath(); ctx.arc(44, y, 6, 0, Math.PI * 2); ctx.stroke();
      if (active) { ctx.fillStyle = '#D97757'; ctx.globalAlpha = 0.5 + 0.5 * Math.sin(sec * 9); ctx.beginPath(); ctx.arc(44, y, 3, 0, Math.PI * 2); ctx.fill(); ctx.globalAlpha = 1; }
    }
  };
  step(136, 'Wi-Fi', wifiOk, !wifiOk);
  step(164, lang === 'en' ? 'Time' : 'Час', timeOk, wifiOk && !timeOk);
  txt(ctx, 'v1.0.0', 120, 230, `500 13px ${SANS}`, '#58524A', 'center', 'alphabetic');
  const c = cell('work', 1, tickAt(sec), { verb: '', over: [], frame: (tickAt(sec) >> 1) % 2 ? 'walkR' : 'right' });
  c.px.forEach((p: Cell, i: number) => {
    if (p.c === 'transparent') return;
    ctx.fillStyle = p.c;
    ctx.fillRect(78 + (i % 14) * 6, 56 + Math.floor(i / 14) * 6, 6, 6);
  });
}
