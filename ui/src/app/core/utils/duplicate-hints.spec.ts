import {
  DUPLICATE_HINT_WINDOW_US,
  DuplicateCandidate,
  isPossibleDuplicate,
  titleForMatching,
} from './duplicate-hints';

const HOUR_US = 3_600 * 1_000_000;

function candidate(fields: Partial<DuplicateCandidate>): DuplicateCandidate {
  return { id: 1, title: 'Leather Night', startsAtUs: 1_000 * HOUR_US, venueId: 1, ...fields };
}

describe('duplicate-hints', () => {
  it('normalizes a title the way the server does', () => {
    expect(titleForMatching('Leather Night!')).toBe('leathernight');
    expect(titleForMatching('  LEATHER-night  ')).toBe('leathernight');
    expect(titleForMatching('Drag Brunch #2')).toBe('dragbrunch2');
    // Beyond ASCII is kept as it is, not lower-cased.
    expect(titleForMatching('Café Ñight')).toBe('caféÑight');
    expect(titleForMatching('!!!')).toBe('');
  });

  it('matches the same venue within 3 hours, either way, inclusive', () => {
    const event = candidate({ id: 2, title: 'Something else' });
    expect(isPossibleDuplicate(event, candidate({}))).toBe(true);
    expect(
      isPossibleDuplicate(
        event,
        candidate({ startsAtUs: event.startsAtUs + DUPLICATE_HINT_WINDOW_US }),
      ),
    ).toBe(true);
    expect(
      isPossibleDuplicate(event, candidate({ startsAtUs: event.startsAtUs - 3 * HOUR_US })),
    ).toBe(true);
    expect(
      isPossibleDuplicate(event, candidate({ startsAtUs: event.startsAtUs + 3 * HOUR_US + 1 })),
    ).toBe(false);
  });

  it('matches the same title at another venue, or with no venue', () => {
    const event = candidate({ id: 2, title: 'leather night', venueId: null });
    expect(isPossibleDuplicate(event, candidate({ venueId: 7 }))).toBe(true);
    expect(isPossibleDuplicate(event, candidate({ title: 'Bear Night', venueId: 7 }))).toBe(false);
  });

  it('never matches on a missing venue or an empty title', () => {
    const event = candidate({ id: 2, title: '???', venueId: null });
    expect(isPossibleDuplicate(event, candidate({ title: '!!!', venueId: null }))).toBe(false);
  });
});
