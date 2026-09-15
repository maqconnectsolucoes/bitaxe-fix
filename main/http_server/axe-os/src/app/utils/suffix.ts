/** Metric suffix letters in power-of-1000 order, shared by the suffix pipes and the parser. */
export const METRIC_SUFFIXES = ['', 'K', 'M', 'G', 'T', 'P', 'E'] as const;

/**
 * Parses a human-formatted number such as "1.5 T" or "123" (the string form of
 * bestDiff/bestSessionDiff on older firmware) back into a plain number.
 */
export function parseSuffixString(input: string): number {
  input = input.trim();
  const value = parseFloat(input);
  const lastChar = input.charAt(input.length - 1).toUpperCase();
  const power = METRIC_SUFFIXES.indexOf(lastChar as typeof METRIC_SUFFIXES[number]);
  return value * Math.pow(1000, power > 0 ? power : 0);
}
