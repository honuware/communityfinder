import { listingCountsText } from './guide-labels';

describe('guide-labels', () => {
  it('counts what a scene or neighborhood lists, leaving out what it has none of', () => {
    expect(listingCountsText({ venue_count: 2, organization_count: 1, place_count: 0 })).toBe(
      '2 venues · 1 organization',
    );
    expect(listingCountsText({ venue_count: 1, organization_count: 0, place_count: 3 })).toBe(
      '1 venue · 3 places',
    );
    expect(listingCountsText({ venue_count: 0, organization_count: 0, place_count: 0 })).toBe('');
  });
});
