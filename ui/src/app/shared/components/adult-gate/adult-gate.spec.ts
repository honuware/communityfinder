import { Component, signal } from '@angular/core';
import { TestBed } from '@angular/core/testing';
import { provideRouter, Router } from '@angular/router';
import { vi } from 'vitest';
import { ADULT_OK_STORAGE_KEY, AdultGate } from './adult-gate';

// Counts how many times the gated body has been created.
let bodiesCreated = 0;

@Component({ selector: 'app-gated-body', template: '<p class="gated-body">Inside</p>' })
class GatedBody {
  constructor() {
    bodiesCreated++;
  }
}

@Component({
  imports: [AdultGate, GatedBody],
  template: `
    <app-adult-gate [adult]="adult()">
      <ng-template><app-gated-body /></ng-template>
    </app-adult-gate>
  `,
})
class Host {
  readonly adult = signal(true);
}

function render(adult = true) {
  TestBed.configureTestingModule({ providers: [provideRouter([])] });
  const fixture = TestBed.createComponent(Host);
  fixture.componentInstance.adult.set(adult);
  fixture.detectChanges();
  const element = fixture.nativeElement as HTMLElement;
  return {
    fixture,
    element,
    gate: () => element.querySelector('.adult-gate'),
    body: () => element.querySelector('.gated-body'),
    click: (selector: string) => {
      element.querySelector<HTMLButtonElement>(selector)!.click();
      fixture.detectChanges();
    },
  };
}

describe('AdultGate', () => {
  beforeEach(() => {
    sessionStorage.clear();
    bodiesCreated = 0;
  });
  afterEach(() => vi.restoreAllMocks());

  it("asks first, and doesn't even create the body until the reader says yes", () => {
    const page = render();
    expect(page.gate()?.textContent).toContain('18+ section');
    expect(page.body()).toBeNull();
    expect(bodiesCreated).toBe(0);
  });

  it('shows the body on yes, and remembers it for this tab only', () => {
    const page = render();
    page.click('.adult-gate__accept');
    expect(page.gate()).toBeNull();
    expect(page.body()?.textContent).toBe('Inside');
    expect(sessionStorage.getItem(ADULT_OK_STORAGE_KEY)).toBe('true');
    expect(localStorage.getItem(ADULT_OK_STORAGE_KEY)).toBeNull();
  });

  it("doesn't ask again once answered in this tab", () => {
    sessionStorage.setItem(ADULT_OK_STORAGE_KEY, 'true');
    const page = render();
    expect(page.gate()).toBeNull();
    expect(page.body()).not.toBeNull();
  });

  it('takes the reader back to the guide on no, still without the body', () => {
    const page = render();
    const navigate = vi.spyOn(TestBed.inject(Router), 'navigateByUrl').mockResolvedValue(true);
    page.click('.adult-gate__decline');
    expect(navigate).toHaveBeenCalledWith('/guide');
    expect(page.body()).toBeNull();
    expect(sessionStorage.getItem(ADULT_OK_STORAGE_KEY)).toBeNull();
  });

  it('passes content that is not adult straight through, without asking', () => {
    const page = render(false);
    expect(page.gate()).toBeNull();
    expect(page.body()).not.toBeNull();
    expect(sessionStorage.getItem(ADULT_OK_STORAGE_KEY)).toBeNull();
  });

  it('still opens on yes when the browser blocks storage', () => {
    vi.spyOn(Storage.prototype, 'getItem').mockImplementation(() => {
      throw new Error('blocked');
    });
    vi.spyOn(Storage.prototype, 'setItem').mockImplementation(() => {
      throw new Error('blocked');
    });
    const page = render();
    expect(page.gate()).not.toBeNull();
    page.click('.adult-gate__accept');
    expect(page.body()).not.toBeNull();
  });
});
