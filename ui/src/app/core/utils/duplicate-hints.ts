// The two "is it the same event?" rules the server's FindDuplicateHints uses
// (EV3 "Duplicate hints"), mirrored so the offline mock can fake the hints and
// the review queue can flag possible duplicates from its own rows. Hints only:
// nothing is ever refused for being similar.

// Two events starting within 3 hours of each other, either way.
export const DUPLICATE_HINT_WINDOW_US = 3 * 3_600 * 1_000_000;

export interface DuplicateCandidate {
  id: number;
  title: string;
  startsAtUs: number;
  venueId: number | null;
}

// "Leather Night!" and "leather night" match: ASCII letters and digits
// lower-cased, other ASCII dropped, anything beyond ASCII kept as it is.
export function titleForMatching(title: string): string {
  let normalized = '';
  for (const character of title) {
    if (character.charCodeAt(0) >= 0x80) {
      normalized += character;
    } else if (/[A-Za-z0-9]/.test(character)) {
      normalized += character.toLowerCase();
    }
  }
  return normalized;
}

// Within the window, and at the same venue or under the same title.
export function isPossibleDuplicate(event: DuplicateCandidate, other: DuplicateCandidate): boolean {
  if (Math.abs(event.startsAtUs - other.startsAtUs) > DUPLICATE_HINT_WINDOW_US) {
    return false;
  }
  const sameVenue = event.venueId !== null && event.venueId === other.venueId;
  const title = titleForMatching(event.title);
  return sameVenue || (title !== '' && titleForMatching(other.title) === title);
}
