import { TestBed } from '@angular/core/testing';
import { provideNoopAnimations } from '@angular/platform-browser/animations';
import { throwError } from 'rxjs';
import { vi } from 'vitest';
import { ToastService } from '@honuware/ui/foundation';
import { EVENTS_ADMIN_ACCESS } from '../../core/events-admin-access/events-admin-access';
import { provideEventsAdminAccess } from '../../core/events-admin-access/events-admin-access.providers';
import { PastEventSettingsMockStore } from '../../core/mock/past-event-settings-mock-store';
import { PastEventSettingsCard } from './past-event-settings-card';

// The settings come from the offline mock, whose store holds the server's
// default and range: 365 days (1–730), the calendar's past events off.
function render(before: (store: PastEventSettingsMockStore) => void = () => undefined) {
  TestBed.configureTestingModule({
    imports: [PastEventSettingsCard],
    providers: [provideNoopAnimations(), provideEventsAdminAccess(true)],
  });
  const store = TestBed.inject(PastEventSettingsMockStore);
  before(store);
  const save = vi.spyOn(TestBed.inject(EVENTS_ADMIN_ACCESS), 'savePastEventSettings');
  const success = vi.spyOn(TestBed.inject(ToastService), 'success');
  const fixture = TestBed.createComponent(PastEventSettingsCard);
  fixture.detectChanges();
  return {
    fixture,
    card: fixture.componentInstance,
    element: fixture.nativeElement as HTMLElement,
    save,
    success,
    store,
  };
}

function text(element: Element | null | undefined): string {
  return element?.textContent?.replace(/\s+/g, ' ').trim() ?? '';
}

function button(element: HTMLElement, label: string): HTMLButtonElement {
  return Array.from(element.querySelectorAll('button')).find((entry) => text(entry) === label)!;
}

function slideToggle(element: HTMLElement): HTMLButtonElement {
  return element.querySelector('mat-slide-toggle button')!;
}

describe('PastEventSettingsCard', () => {
  afterEach(() => vi.restoreAllMocks());

  it('shows the window with its default and range, and the calendar switch', async () => {
    const { fixture, element } = render((store) =>
      store.save({ archived_events_visible_days: 90 }),
    );
    await fixture.whenStable();
    expect(element.querySelector('input')?.value).toBe('90');
    expect(text(element.querySelector('mat-hint'))).toBe('Default 365 · allowed 1 to 730');
    expect(text(element.querySelector('mat-slide-toggle'))).toBe(
      'Calendar shows past events at first',
    );
    expect(slideToggle(element).getAttribute('aria-checked')).toBe('false');
  });

  it('saves the window only once it changed, and says what it means', () => {
    const { fixture, card, element, save, success, store } = render();
    expect(button(element, 'Save').disabled).toBe(true);

    card.form.controls.visibleDays.setValue(30);
    fixture.detectChanges();
    button(element, 'Save').click();
    fixture.detectChanges();

    expect(save).toHaveBeenCalledWith({ archived_events_visible_days: 30 });
    expect(store.visibleDays).toBe(30);
    expect(success).toHaveBeenCalledWith(
      'Saved. An event now stays on the site for 30 days after it ends.',
    );
    expect(button(element, 'Save').disabled).toBe(true);
  });

  it('fills in the default on reset, for a Save to apply', () => {
    const { fixture, card, element, save } = render((store) =>
      store.save({ archived_events_visible_days: 30 }),
    );
    button(element, 'Reset to default').click();
    fixture.detectChanges();
    expect(card.form.controls.visibleDays.value).toBe(365);
    expect(save).not.toHaveBeenCalled();
    button(element, 'Save').click();
    expect(save).toHaveBeenCalledWith({ archived_events_visible_days: 365 });
  });

  it('catches an out-of-range or blank window before sending', () => {
    const { fixture, card, element, save } = render();
    for (const value of [0, 731, null]) {
      card.form.controls.visibleDays.setValue(value);
      fixture.detectChanges();
      button(element, 'Save').click();
      fixture.detectChanges();
      expect(text(element.querySelector('.past-event-settings__error'))).toBe(
        'Enter a whole number from 1 to 730.',
      );
    }
    expect(save).not.toHaveBeenCalled();
  });

  it('shows the server’s range refusal on the field', () => {
    const { fixture, card, element, save } = render();
    save.mockReturnValueOnce(
      throwError(() => ({
        status: 400,
        error_code: 'DAYS_OUT_OF_RANGE',
        field: 'archived_events_visible_days',
        date_index: null,
        detail: 'archived_events_visible_days is outside its allowed range',
      })),
    );
    card.form.controls.visibleDays.setValue(60);
    fixture.detectChanges();
    button(element, 'Save').click();
    fixture.detectChanges();
    expect(
      text(
        element.querySelector(
          '[data-field="archived_events_visible_days"] .past-event-settings__error',
        ),
      ),
    ).toBe('Enter a whole number from 1 to 730.');
  });

  it('saves the switch on its own, leaving a window being edited as typed', () => {
    const { fixture, card, element, save, success, store } = render();
    card.form.controls.visibleDays.setValue(45);
    fixture.detectChanges();

    slideToggle(element).click();
    fixture.detectChanges();
    expect(save).toHaveBeenCalledTimes(1);
    expect(save).toHaveBeenCalledWith({ events_show_past_by_default: true });
    expect(store.publicSettings()).toEqual({
      archived_events_visible_days: 365,
      events_show_past_by_default: true,
    });
    expect(slideToggle(element).getAttribute('aria-checked')).toBe('true');
    expect(card.form.controls.visibleDays.value).toBe(45);
    expect(success).toHaveBeenCalledWith(
      'The calendar now shows past events until a visitor turns them off.',
    );
  });

  it('puts the switch back when it could not be saved', () => {
    const { fixture, element, save } = render();
    const error = vi.spyOn(TestBed.inject(ToastService), 'error');
    save.mockReturnValueOnce(throwError(() => new Error('offline')));
    slideToggle(element).click();
    fixture.detectChanges();
    expect(slideToggle(element).getAttribute('aria-checked')).toBe('false');
    expect(error).toHaveBeenCalledWith("The switch couldn't be saved. Please try again.");
  });
});
