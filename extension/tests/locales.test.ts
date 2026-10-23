import { describe, it, expect } from 'vitest';
import { savedLocale } from '../../shared/locales';

describe('language preference upgrades', () => {
  it('starts in English and upgrades the former implicit Bulgarian default', () => {
    expect(savedLocale(undefined)).toBe('en');
    expect(savedLocale({ locale: 'bg' })).toBe('en');
  });
  it('retains an explicit supported preference after an extension restart', () => {
    expect(savedLocale({ localeVersion: 1, locale: 'fr' })).toBe('fr');
    expect(savedLocale({ localeVersion: 1, locale: 'bg' })).toBe('bg');
  });
  it('recovers to English from an invalid stored preference', () => {
    expect(savedLocale({ localeVersion: 1, locale: '__proto__' })).toBe('en');
    expect(savedLocale({ localeVersion: 1, locale: null })).toBe('en');
  });
});
