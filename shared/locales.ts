export const languages = [
  { code: 'en', name: 'English' },
  { code: 'bg', name: 'Български' },
  { code: 'de', name: 'Deutsch' },
  { code: 'es', name: 'Español' },
  { code: 'fr', name: 'Français' },
] as const;

export type Locale = (typeof languages)[number]['code'];
export const DEFAULT_LOCALE: Locale = 'en';

export function isLocale(value: unknown): value is Locale {
  return languages.some(({ code }) => code === value);
}

export function getLocale(value: unknown): Locale {
  return isLocale(value) ? value : DEFAULT_LOCALE;
}

// Before the language selector, Bulgarian was an implicit default. Upgrade once
// to English; subsequent explicit selections survive restarts and updates.
export function savedLocale(saved: unknown): Locale {
  if (!saved || typeof saved !== 'object') return DEFAULT_LOCALE;
  const preference = saved as { locale?: unknown; localeVersion?: unknown };
  return preference.localeVersion === 1 ? getLocale(preference.locale) : DEFAULT_LOCALE;
}
