import { openGuide, text, texts, useGuideTestClock } from './guide-testing';

function pressed(element: HTMLElement): string[] {
  return texts(element, '.organization-directory__kind[aria-pressed="true"]');
}

describe('OrganizationDirectoryPage', () => {
  useGuideTestClock();

  it('lists the active organizations by name, with their kinds', async () => {
    const { element } = await openGuide('/guide/orgs');
    expect(text(element.querySelector('h1'))).toBe('Organizations');
    expect(texts(element, '.entity-card__name')).toEqual([
      'Cascade Queer Softball League',
      'Clear Path Recovery Circle',
      'Harbor Voices Chorus',
      'Lakeside Queer Professionals',
      'Open Table Fellowship',
      'Pacific Leather Collective',
      'Rolling Dice Gamers',
    ]);
    expect(texts(element, '.organization-directory__kind')).toHaveLength(8);
    expect(pressed(element)).toEqual([]);
  });

  it('filters by kind from the address, and a kind button toggles it', async () => {
    const { element, settle, router } = await openGuide('/guide/orgs?kind=recovery');
    expect(texts(element, '.entity-card__name')).toEqual(['Clear Path Recovery Circle']);
    expect(pressed(element)).toEqual(['Recovery']);

    const faith = Array.from(
      element.querySelectorAll<HTMLButtonElement>('.organization-directory__kind'),
    ).find((button) => text(button) === 'Faith')!;
    faith.click();
    await settle();
    expect(router.url).toBe('/guide/orgs?kind=faith');
    expect(texts(element, '.entity-card__name')).toEqual(['Open Table Fellowship']);
    expect(pressed(element)).toEqual(['Faith']);

    // Picking it again turns it off.
    faith.click();
    await settle();
    expect(router.url).toBe('/guide/orgs');
    expect(pressed(element)).toEqual([]);
  });

  it('combines kind, scene and neighborhood', async () => {
    const { element } = await openGuide('/guide/orgs?kind=club-hobby&neighborhood=bellevue');
    expect(texts(element, '.entity-card__name')).toEqual(['Rolling Dice Gamers']);
  });
});
