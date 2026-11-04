import { TestBed } from '@angular/core/testing';
import { provideRouter } from '@angular/router';
import { Footer } from './footer';

describe('Footer', () => {
  function render(): HTMLElement {
    TestBed.configureTestingModule({ imports: [Footer], providers: [provideRouter([])] });
    const fixture = TestBed.createComponent(Footer);
    fixture.detectChanges();
    return fixture.nativeElement as HTMLElement;
  }

  it('links to the site’s sections, and to the submit form', () => {
    const links = Array.from(render().querySelectorAll('.app-footer__links a')).map((link) => [
      link.textContent?.trim(),
      link.getAttribute('href'),
    ]);
    expect(links).toEqual([
      ['Events', '/events'],
      ['Calendar', '/events/calendar'],
      ['Recurring', '/events/series'],
      ['Guide', '/guide'],
      ['Venues', '/guide/venues'],
      ['Organizations', '/guide/orgs'],
      ['Submit an event', '/events/submit'],
    ]);
  });

  it('says the form is the only way listings come in', () => {
    const intake = render().querySelector('.app-footer__intake')!;
    expect(intake.textContent?.replace(/\s+/g, ' ').trim()).toBe(
      "Know of an event? Submit it here. It's the only way we take listings.",
    );
    expect(intake.querySelector('a')?.getAttribute('href')).toBe('/events/submit');
  });

  it('shows the site name and the year', () => {
    const element = render();
    expect(element.querySelector('.app-footer__name')?.textContent).toBe('CommunityFinder');
    expect(element.querySelector('.app-footer__legal')?.textContent).toContain(
      String(new Date().getFullYear()),
    );
  });
});
