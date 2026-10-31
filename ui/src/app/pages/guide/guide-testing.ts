import { Provider } from '@angular/core';
import { TestBed } from '@angular/core/testing';
import { provideNoopAnimations } from '@angular/platform-browser/animations';
import { provideRouter, Router } from '@angular/router';
import { RouterTestingHarness } from '@angular/router/testing';
import { HONUWARE_API_BASE } from '@honuware/ui/access';
import { afterEach, beforeEach, vi } from 'vitest';
import { guideRoutes } from './guide.routes';

// Shared set-up for the guide page specs: the real /guide routes over the
// offline mock data, as of a fixed "now". Not a .spec.ts, so the app build
// type-checks it too (nothing imports it there): hence the explicit vitest
// imports rather than the test globals.

// Thu Oct 29, 2026, 1 PM in Seattle.
export const GUIDE_NOW_US = Date.UTC(2026, 9, 29, 20) * 1000;

// Freezes the clock (Date only; timers stay real) and starts each test with
// no 18+ answer stored.
export function useGuideTestClock(): void {
  beforeEach(() => {
    vi.useFakeTimers({ toFake: ['Date'] });
    vi.setSystemTime(new Date(GUIDE_NOW_US / 1000));
    sessionStorage.clear();
  });
  afterEach(() => {
    vi.useRealTimers();
    vi.restoreAllMocks();
  });
}

export function configureGuide(providers: Provider[] = []): void {
  TestBed.configureTestingModule({
    providers: [
      provideRouter([{ path: 'guide', children: guideRoutes(true) }]),
      provideNoopAnimations(),
      { provide: HONUWARE_API_BASE, useValue: '/api' },
      ...providers,
    ],
  });
}

// Opens a guide address and returns the rendered page.
export async function openGuide(url: string) {
  configureGuide();
  const harness = await RouterTestingHarness.create();
  await harness.navigateByUrl(url);
  return {
    harness,
    element: harness.routeNativeElement as HTMLElement,
    router: TestBed.inject(Router),
    // After a click that navigates: let the navigation finish, then re-render.
    settle: async () => {
      await harness.fixture.whenStable();
      harness.detectChanges();
    },
    // Markdown renders asynchronously: wait until `selector` shows up.
    waitFor: (selector: string) =>
      vi.waitFor(() => {
        harness.detectChanges();
        const found = (harness.routeNativeElement as HTMLElement).querySelector(selector);
        if (!found) {
          throw new Error(`${selector} hasn't rendered`);
        }
        return found;
      }),
  };
}

export function text(element: Element | null | undefined): string {
  return element?.textContent?.replace(/\s+/g, ' ').trim() ?? '';
}

export function texts(element: Element, selector: string): string[] {
  return Array.from(element.querySelectorAll(selector)).map(text);
}

// The anchor whose text is exactly `label`.
export function link(element: Element, label: string): HTMLAnchorElement | undefined {
  return Array.from(element.querySelectorAll('a')).find((anchor) => text(anchor) === label);
}
