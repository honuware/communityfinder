import { Component } from '@angular/core';
import { TestBed } from '@angular/core/testing';
import { provideRouter, Router } from '@angular/router';
import { RouterTestingHarness } from '@angular/router/testing';
import { provideNoopAnimations } from '@angular/platform-browser/animations';
import { throwError } from 'rxjs';
import { vi } from 'vitest';
import { AuthService } from '@honuware/ui/auth';
import { ToastService } from '@honuware/ui/foundation';
import { routes as appRoutes } from '../../app.routes';
import { EVENTS_ADMIN_ACCESS } from '../../core/events-admin-access/events-admin-access';
import { provideEventsAdminAccess } from '../../core/events-admin-access/events-admin-access.providers';
import { SubmissionSettingsMockStore } from '../../core/mock/submission-settings-mock-store';
import { SubmissionSettingsPage } from './submission-settings';

// The settings come from the offline mock, whose store holds the server's
// defaults and ranges: max_pending_dates 10 (1–100), max_submissions_per_day
// 5 (1–50), not paused.
function render(before: (store: SubmissionSettingsMockStore) => void = () => undefined) {
  TestBed.configureTestingModule({
    imports: [SubmissionSettingsPage],
    providers: [provideRouter([]), provideNoopAnimations(), provideEventsAdminAccess(true)],
  });
  const store = TestBed.inject(SubmissionSettingsMockStore);
  before(store);
  const access = TestBed.inject(EVENTS_ADMIN_ACCESS);
  const save = vi.spyOn(access, 'saveSubmissionSettings');
  const fixture = TestBed.createComponent(SubmissionSettingsPage);
  fixture.detectChanges();
  return {
    fixture,
    page: fixture.componentInstance,
    element: fixture.nativeElement as HTMLElement,
    save,
    store,
  };
}

function text(element: Element | null | undefined): string {
  return element?.textContent?.replace(/\s+/g, ' ').trim() ?? '';
}

function button(element: HTMLElement, label: string): HTMLButtonElement {
  return Array.from(element.querySelectorAll('button')).find((entry) => text(entry) === label)!;
}

function input(element: HTMLElement, field: string): HTMLInputElement {
  return element.querySelector(`[data-field="${field}"] input`)!;
}

function slideToggle(element: HTMLElement): HTMLButtonElement {
  return element.querySelector('mat-slide-toggle button')!;
}

describe('SubmissionSettingsPage', () => {
  afterEach(() => vi.restoreAllMocks());

  describe('the limits', () => {
    it('shows each value with its default and range', async () => {
      const { fixture, element } = render((store) => store.save({ max_pending_dates: 20 }));
      await fixture.whenStable();
      expect(input(element, 'max_pending_dates').value).toBe('20');
      expect(input(element, 'max_submissions_per_day').value).toBe('5');
      expect(text(element.querySelector('[data-field="max_pending_dates"] mat-hint'))).toBe(
        'Default 10 · allowed 1 to 100',
      );
      expect(text(element.querySelector('[data-field="max_submissions_per_day"] mat-hint'))).toBe(
        'Default 5 · allowed 1 to 50',
      );
      expect(text(element.querySelector('[data-field="max_pending_dates"]'))).toContain(
        'Dates one person can have waiting for review',
      );
    });

    it('enables Save only once something changed, and sends only that', () => {
      const { fixture, page, element, save, store } = render();
      const success = vi.spyOn(TestBed.inject(ToastService), 'success');
      expect(button(element, 'Save').disabled).toBe(true);

      page.form.controls.maxSubmissionsPerDay.setValue(8);
      fixture.detectChanges();
      expect(button(element, 'Save').disabled).toBe(false);
      button(element, 'Save').click();
      fixture.detectChanges();

      expect(save).toHaveBeenCalledTimes(1);
      expect(save).toHaveBeenCalledWith({ max_submissions_per_day: 8 });
      expect(store.maxSubmissionsPerDay).toBe(8);
      expect(store.maxPendingDates).toBe(10);
      expect(success).toHaveBeenCalledWith('Saved. The new limits apply to the next submission.');
      // Saved: nothing differs any more.
      expect(button(element, 'Save').disabled).toBe(true);
    });

    it('fills in the defaults on reset, for a Save to apply', () => {
      const { fixture, page, element, save } = render((store) =>
        store.save({ max_pending_dates: 40, max_submissions_per_day: 2 }),
      );
      button(element, 'Reset to defaults').click();
      fixture.detectChanges();
      expect(page.form.getRawValue()).toEqual({ maxPendingDates: 10, maxSubmissionsPerDay: 5 });
      expect(save).not.toHaveBeenCalled();
      button(element, 'Save').click();
      expect(save).toHaveBeenCalledWith({ max_pending_dates: 10, max_submissions_per_day: 5 });
    });

    it('catches an out-of-range value before sending, beside its field', () => {
      const { fixture, page, element, save } = render();
      page.form.controls.maxPendingDates.setValue(0);
      fixture.detectChanges();
      button(element, 'Save').click();
      fixture.detectChanges();
      expect(save).not.toHaveBeenCalled();
      expect(
        text(element.querySelector('[data-field="max_pending_dates"] .submission-settings__error')),
      ).toBe('Enter a whole number from 1 to 100.');
    });

    it('shows the server’s range refusal on the field it names', () => {
      const { fixture, page, element, save } = render();
      save.mockReturnValueOnce(
        throwError(() => ({
          status: 400,
          error_code: 'LIMIT_OUT_OF_RANGE',
          field: 'max_submissions_per_day',
          date_index: null,
          detail: 'max_submissions_per_day must be from 1 to 50',
        })),
      );
      page.form.controls.maxPendingDates.setValue(12);
      page.form.controls.maxSubmissionsPerDay.setValue(9);
      fixture.detectChanges();
      button(element, 'Save').click();
      fixture.detectChanges();
      expect(
        text(
          element.querySelector(
            '[data-field="max_submissions_per_day"] .submission-settings__error',
          ),
        ),
      ).toBe('Enter a whole number from 1 to 50.');
      expect(
        element.querySelector('[data-field="max_pending_dates"] .submission-settings__error'),
      ).toBeNull();
    });
  });

  describe('the pause switch', () => {
    it('confirms in the page before pausing, then sends only the switch', () => {
      const { fixture, page, element, save, store } = render();
      expect(text(element.querySelector('mat-slide-toggle'))).toBe('Taking submissions');
      page.form.controls.maxPendingDates.setValue(30);

      slideToggle(element).click();
      fixture.detectChanges();
      const confirm = element.querySelector('[role="alertdialog"]')!;
      expect(text(confirm)).toContain(
        'Nobody will be able to submit an event until you turn this back on.',
      );
      expect(save).not.toHaveBeenCalled();
      expect(page.paused()).toBe(false);
      // The switch stays on until it's confirmed.
      expect(slideToggle(element).getAttribute('aria-checked')).toBe('true');

      button(element, 'Pause submissions').click();
      fixture.detectChanges();
      expect(save).toHaveBeenCalledTimes(1);
      expect(save).toHaveBeenCalledWith({ submissions_paused: true });
      expect(store.paused).toBe(true);
      expect(store.maxPendingDates).toBe(10);
      expect(text(element.querySelector('mat-slide-toggle'))).toBe('Paused');
      // The limit being edited stays as typed.
      expect(page.form.controls.maxPendingDates.value).toBe(30);
      expect(element.querySelector('[role="alertdialog"]')).toBeNull();
    });

    it('stays on when the pause is cancelled', () => {
      const { fixture, element, save, store } = render();
      slideToggle(element).click();
      fixture.detectChanges();
      button(element, 'Cancel').click();
      fixture.detectChanges();
      expect(save).not.toHaveBeenCalled();
      expect(store.paused).toBe(false);
      expect(element.querySelector('[role="alertdialog"]')).toBeNull();
      expect(slideToggle(element).getAttribute('aria-checked')).toBe('true');
    });

    it('turns submissions back on without asking', () => {
      const { fixture, element, save, store } = render((store) =>
        store.save({ submissions_paused: true }),
      );
      expect(text(element.querySelector('mat-slide-toggle'))).toBe('Paused');
      slideToggle(element).click();
      fixture.detectChanges();
      expect(element.querySelector('[role="alertdialog"]')).toBeNull();
      expect(save).toHaveBeenCalledWith({ submissions_paused: false });
      expect(store.paused).toBe(false);
      expect(text(element.querySelector('mat-slide-toggle'))).toBe('Taking submissions');
    });

    it('is left alone by "Reset to defaults" and by Save', () => {
      const { fixture, page, element, save, store } = render((store) =>
        store.save({ submissions_paused: true, max_pending_dates: 40 }),
      );
      button(element, 'Reset to defaults').click();
      fixture.detectChanges();
      expect(page.paused()).toBe(true);
      button(element, 'Save').click();
      expect(save).toHaveBeenCalledWith({ max_pending_dates: 10 });
      expect(store.paused).toBe(true);
    });
  });
});

@Component({ template: '' })
class Blank {}

// The app's own /admin entry, with its guards; the pages it redirects to are
// stand-ins.
describe('SubmissionSettingsPage route', () => {
  async function visit(authData: object): Promise<string> {
    TestBed.configureTestingModule({
      providers: [
        provideRouter([
          { path: '', component: Blank },
          { path: 'login', component: Blank },
          appRoutes.find((route) => route.path === 'admin')!,
        ]),
        provideNoopAnimations(),
        provideEventsAdminAccess(true),
        { provide: AuthService, useValue: { authData } },
      ],
    });
    const harness = await RouterTestingHarness.create();
    await harness.navigateByUrl('/admin/events/settings');
    return TestBed.inject(Router).url;
  }

  it('sends a signed-out visitor to sign in, and back', async () => {
    expect(await visit({ isAuth: false })).toBe('/login?returnUrl=%2Fadmin%2Fevents%2Fsettings');
  });

  it('sends a member who isn’t a curator home', async () => {
    expect(await visit({ isAuth: true, personId: 2, isAdmin: false })).toBe('/');
  });

  it('opens for a curator', async () => {
    expect(await visit({ isAuth: true, personId: 1, isAdmin: true })).toBe(
      '/admin/events/settings',
    );
  });
});
