import { en } from './en';
import { fr } from './fr';

type Val = string | string[] | string[][] | Dict;
type Dict = { [k: string]: Val };
const LANGS: Record<string, Dict> = { fr, en };
let lang = 'fr';

export function setLang(l: string): void {
  lang = LANGS[l] ? l : 'fr';
  document.documentElement.lang = lang;
}

export function getLang(): string {
  return lang;
}

function lookup(d: Dict, key: string): Val | undefined {
  let cur: Val | undefined = d;
  for (const p of key.split('.')) {
    if (!cur || typeof cur === 'string' || Array.isArray(cur)) return undefined;
    cur = cur[p];
  }
  return cur;
}

/** Translated text; {name} placeholders are filled from vars. */
export function t(key: string, vars: Record<string, string | number> = {}): string {
  const v = lookup(LANGS[lang], key) ?? lookup(fr, key);
  if (typeof v !== 'string') return key;
  return v.replace(/\{(\w+)\}/g, (_, k) => String(vars[k] ?? `{${k}}`));
}

export function tl(key: string): string[] {
  const v = lookup(LANGS[lang], key) ?? lookup(fr, key);
  return Array.isArray(v) ? (v as string[]) : [];
}

/** Dialogue lines: [speaker, text][]. */
export function dialogue(key: string): [string, string][] {
  const v = lookup(LANGS[lang], key) ?? lookup(fr, key);
  return Array.isArray(v) ? (v as unknown as [string, string][]) : [];
}

export function has(key: string): boolean {
  return lookup(LANGS[lang], key) !== undefined;
}
