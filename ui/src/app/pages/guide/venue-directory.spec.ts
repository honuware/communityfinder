import { openGuide, text, texts, useGuideTestClock } from './guide-testing';

describe('VenueDirectoryPage', () => {
  useGuideTestClock();

  it('lists the open venues by name, with their neighborhoods', async () => {
    const { element } = await openGuide('/guide/venues');
    expect(text(element.querySelector('h1'))).toBe('Venues');
    expect(texts(element, '.entity-card__name')).toEqual([
      'Columbia City Social Club',
      'Driftwood Hall',
      'Eastside Commons',
      'Iron & Oak',
      'The Lantern Room',
    ]);
    expect(texts(element, '.entity-card__meta')).toEqual([
      'Columbia City',
      'Ballard',
      'Bellevue',
      'Capitol Hill',
      'Capitol Hill',
    ]);
  });

  it('filters by what the address says, and can bring closed venues back', async () => {
    const bears = await openGuide('/guide/venues?scene=bear');
    expect(texts(bears.element, '.entity-card__name')).toEqual(['The Lantern Room']);

    await bears.harness.navigateByUrl('/guide/venues?scene=bear&include_closed=true');
    expect(texts(bears.element, '.entity-card__name')).toEqual(['The Lantern Room', 'The Wren']);
    expect(texts(bears.element, '.entity-card__badge')).toEqual(['Closed']);

    await bears.harness.navigateByUrl('/guide/venues?scene=furry');
    expect(text(bears.element.querySelector('.guide-page__empty'))).toContain(
      'No venues match these filters.',
    );
  });

  it('puts the "include closed" choice in the address', async () => {
    const { element, settle, router } = await openGuide('/guide/venues?scene=bear');
    element.querySelector<HTMLInputElement>('.venue-directory__include-closed input')!.click();
    await settle();
    expect(router.url).toBe('/guide/venues?scene=bear&include_closed=true');
    expect(texts(element, '.entity-card__name')).toEqual(['The Lantern Room', 'The Wren']);
  });
});
