import { link, openGuide, text, texts, useGuideTestClock } from './guide-testing';

describe('NeighborhoodsPage', () => {
  useGuideTestClock();

  it("groups the neighborhoods by GD1's regions, the Eastside its own", async () => {
    const { element } = await openGuide('/guide/neighborhoods');
    expect(text(element.querySelector('h1'))).toBe('Neighborhoods');
    expect(texts(element, '.neighborhoods-page__region .guide-page__section-title')).toEqual([
      'Seattle',
      'South Seattle',
      'North of Seattle',
      'Eastside',
      'South King County',
    ]);
    const eastside = element.querySelectorAll('.neighborhoods-page__region')[3];
    expect(texts(eastside, 'a')).toEqual([
      'Bellevue',
      'Kirkland',
      'Redmond',
      'Bothell',
      'Issaquah',
      'Sammamish',
    ]);
  });

  it('says what each neighborhood has listed, and links to its page', async () => {
    const { element } = await openGuide('/guide/neighborhoods');
    const bellevue = link(element, 'Bellevue')!;
    expect(bellevue.getAttribute('href')).toBe('/guide/neighborhoods/bellevue');
    expect(text(bellevue.parentElement!.querySelector('.guide-page__list-note'))).toBe(
      '1 venue · 2 organizations',
    );
    // Nothing listed: no count at all.
    expect(
      link(element, 'Kirkland')!.parentElement!.querySelector('.guide-page__list-note'),
    ).toBeNull();
  });
});
