import { en } from './en';
import { fr } from './fr';

type Dict = { [k: string]: string | string[] | Dict };
const LANGS: Record<string, Dict> = { fr, en };
let lang = 'fr';

export function setLang(l: string): void {
  lang = LANGS[l] ? l : 'fr';
  document.documentElement.lang = lang;
}

export function getLang(): string {
  return lang;
}

function lookup(d: Dict, key: string): string | string[] | Dict | undefined {
  let cur: string | string[] | Dict | undefined = d;
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
  return Array.isArray(v) ? v : [];
}

export function has(key: string): boolean {
  return lookup(LANGS[lang], key) !== undefined;
}
