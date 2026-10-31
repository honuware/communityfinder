import { openGuide, text, texts, useGuideTestClock } from './guide-testing';

describe('ServicesPage', () => {
  useGuideTestClock();

  it('groups the services by kind, health first, everyday last', async () => {
    const { element } = await openGuide('/guide/services');
    expect(text(element.querySelector('h1'))).toBe('Services');
    expect(texts(element, '.services-page__group .guide-page__section-title')).toEqual([
      'STI testing',
      'PrEP',
      'Mental health',
      'Gender-affirming care',
      'Dental',
      'Everyday services',
    ]);
    const groups = element.querySelectorAll('.services-page__group');
    // Services link out, so each name carries the "opens elsewhere" icon.
    expect(texts(groups[0], '.entity-card__name')).toEqual([
      'Neighborhood Health Clinic open_in_new',
    ]);
    expect(texts(groups[5], '.entity-card__name')).toEqual(['Northside Barbers open_in_new']);
  });

  it('links each service out to its own site, with its phone number', async () => {
    const { element } = await openGuide('/guide/services');
    const clinic = element.querySelector('.services-page__group app-entity-card')!;
    const name = clinic.querySelector('.entity-card__name')!;
    expect(name.getAttribute('href')).toBe('https://example.org/neighborhood-health-clinic');
    expect(name.getAttribute('target')).toBe('_blank');
    expect(text(clinic.querySelector('.entity-card__phone'))).toBe('206-555-0601');
    expect(text(clinic.querySelector('.entity-card__meta'))).toBe('STI testing · Capitol Hill');
  });
});
