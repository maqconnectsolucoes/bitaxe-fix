import { parseSuffixString } from './suffix';
import { DiffSuffixPipe } from 'src/app/pipes/diff-suffix.pipe';

describe('parseSuffixString', () => {
  it('parses plain numbers', () => expect(parseSuffixString('123')).toBe(123));
  it('parses suffixed values', () => {
    expect(parseSuffixString('1.5 T')).toBe(1.5e12);
    expect(parseSuffixString('2k')).toBe(2e3);
    expect(parseSuffixString(' 7 G ')).toBe(7e9);
  });
  it('round-trips the diffSuffix pipe', () => {
    expect(parseSuffixString(DiffSuffixPipe.transform(2.5e9))).toBeCloseTo(2.5e9, -6);
  });
});
