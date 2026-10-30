import { Component } from '@angular/core';
import { TestBed } from '@angular/core/testing';
import { provideRouter, Router } from '@angular/router';
import { vi } from 'vitest';
import { InAppLinks } from './in-app-links';

// Stands in for a rendered markdown body.
@Component({
  imports: [InAppLinks],
  template: `
    <div appInAppLinks>
      <a id="venue" href="/guide/venues/the-lantern-room?tab=hours#map">The Lantern Room</a>
      <a id="nested" href="/guide/scenes/bear"><strong>Bears</strong></a>
      <a id="external" href="https://example.org/elsewhere">elsewhere</a>
      <a id="mail" href="mailto:hello@example.org">mail</a>
      <a id="phone" href="tel:2065550100">phone</a>
      <a id="new-tab" href="/guide/graveyard" target="_blank">new tab</a>
      <a id="download" href="/guide/graveyard" download>download</a>
      <a id="jump" href="#hours">jump</a>
      <a id="feed" href="/api/events/feed.ics">feed</a>
    </div>
  `,
})
class Host {}

function setUp() {
  TestBed.configureTestingModule({ providers: [provideRouter([])] });
  const navigate = vi.spyOn(TestBed.inject(Router), 'navigateByUrl').mockResolvedValue(true);
  const fixture = TestBed.createComponent(Host);
  fixture.detectChanges();
  const element = fixture.nativeElement as HTMLElement;

  // Clicks the element and reports whether the directive took the click over.
  // Every click is then cancelled at the document, so jsdom doesn't try to
  // follow the links the directive leaves alone.
  const click = (selector: string, init: MouseEventInit = {}): boolean => {
    let handled = false;
    const record = (event: Event) => {
      handled = event.defaultPrevented;
      event.preventDefault();
    };
    document.addEventListener('click', record);
    element
      .querySelector(selector)!
      .dispatchEvent(new MouseEvent('click', { bubbles: true, cancelable: true, ...init }));
    document.removeEventListener('click', record);
    return handled;
  };
  return { navigate, click };
}

describe('InAppLinks', () => {
  afterEach(() => vi.restoreAllMocks());

  it('follows a link to an app page in-app, keeping its query and #anchor', () => {
    const page = setUp();
    expect(page.click('#venue')).toBe(true);
    expect(page.navigate).toHaveBeenCalledWith('/guide/venues/the-lantern-room?tab=hours#map');
  });

  it('works for a click on something inside the link', () => {
    const page = setUp();
    expect(page.click('#nested strong')).toBe(true);
    expect(page.navigate).toHaveBeenCalledWith('/guide/scenes/bear');
  });

  it('leaves other sites, mail and phone links to the browser', () => {
    const page = setUp();
    expect(page.click('#external')).toBe(false);
    expect(page.click('#mail')).toBe(false);
    expect(page.click('#phone')).toBe(false);
    expect(page.navigate).not.toHaveBeenCalled();
  });

  it('leaves modified and middle clicks alone (new tab, new window, save)', () => {
    const page = setUp();
    expect(page.click('#venue', { ctrlKey: true })).toBe(false);
    expect(page.click('#venue', { metaKey: true })).toBe(false);
    expect(page.click('#venue', { shiftKey: true })).toBe(false);
    expect(page.click('#venue', { altKey: true })).toBe(false);
    expect(page.click('#venue', { button: 1 })).toBe(false);
    expect(page.navigate).not.toHaveBeenCalled();
  });

  it("leaves new-tab, download, same-page and the server's /api links alone", () => {
    const page = setUp();
    expect(page.click('#new-tab')).toBe(false);
    expect(page.click('#download')).toBe(false);
    expect(page.click('#jump')).toBe(false);
    expect(page.click('#feed')).toBe(false);
    expect(page.navigate).not.toHaveBeenCalled();
  });
});
