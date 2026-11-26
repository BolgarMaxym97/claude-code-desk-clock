// The clock screens used in the video, in English (or Ukrainian where a scene shows the second language).
import { cell, tickAt, Cell, Pack } from './screen';

export type Lang = 'en' | 'uk';
const DATE = { en: 'Thu 26 Nov', uk: 'Чт 26 лист' };
const TIME = '10:42';

const WORK: Record<string, { n: number; en: [string, string, string]; uk: [string, string, string] }> = {
  bash: { n: 2, en: ['Running', 'npm test', '1 min · main'], uk: ['Виконує', 'npm test', '1 хв · main'] },
  edit: { n: 3, en: ['Editing', 'parser.ts', '3 min · main'], uk: ['Редагує', 'parser.ts', '3 хв · main'] },
  think: { n: 7, en: ['Thinking', '“add tests…”', 'just now · main'], uk: ['Думає', '«додай тести…»', 'щойно · main'] },
  agents: { n: 5, en: ['Delegating', '3 agents', '4 min · main'], uk: ['Делегує', '3 агенти', '4 хв · main'] },
  permission: { n: 9, en: ['Allow?', 'Bash: npm publish', 'waiting 1 min'], uk: ['Дозвіл?', 'Bash: npm publish', 'чекає 1 хв'] },
  yourturn: { n: 10, en: ['Your turn', 'my-app', '2 min ago'], uk: ['Твоя черга', 'my-app', '2 хв тому'] },
  done: { n: 11, en: ['Done', 'in 12 min', 'main'], uk: ['Готово', 'за 12 хв', 'main'] },
};

export function work(kind: keyof typeof WORK, sec: number, lang: Lang = 'en'): Cell {
  const w = WORK[kind];
  const [verb, target, detail] = w[lang];
  const t = tickAt(sec);
  return cell('work', w.n, t, { verb, target, detail, topL: TIME, g5: 46, g5t: '46', g7: 28, g7t: '28',
    frameRing: kind === 'permission' && t % 4 < 2 });
}

export function limits(sec: number, lang: Lang = 'en'): Cell {
  const t = tickAt(sec);
  const fill = Math.min(82, 40 + sec * 30);  // the 5h bar fills up during the shot
  const over = fill >= 70;
  return cell('reset', 7, t, { topL: DATE[lang], time: TIME, g5: fill, g5t: String(Math.round(fill)),
    subOn: over, paceOn: over, g5s1: lang === 'en' ? '1h' : '1г', g5s2: lang === 'en' ? '05m' : '05х',
    g7: 64, g7t: '64' });
}

const CARD_EN: Record<string, string> = { 'Гелловін': 'Halloween', 'Годиннику 1 рік!': 'Clock is 1 today!', 'День Незалежності': 'Independence Day' };

export function idle(pack: Pack, num: number, sec: number, lang: Lang = 'en', time = TIME): Cell {
  const c = cell(pack, num, tickAt(sec), { topL: DATE[lang], time });
  if (c.cardOn && lang === 'en') {
    if (CARD_EN[c.cardL]) c.cardL = CARD_EN[c.cardL]; else c.cardOn = false;
  }
  return c;
}

export const MONTAGE: [Pack, number][] = [['pack', 1], ['pack', 13], ['pack2', 52], ['pack3', 78], ['pack', 25], ['pack2', 40], ['pack3', 75]];
