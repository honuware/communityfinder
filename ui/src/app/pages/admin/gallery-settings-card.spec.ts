import { TestBed } from '@angular/core/testing';
import { provideNoopAnimations } from '@angular/platform-browser/animations';
import { throwError } from 'rxjs';
import { vi } from 'vitest';
import { ToastService } from '@honuware/ui/foundation';
import { EVENTS_ADMIN_ACCESS } from '../../core/events-admin-access/events-admin-access';
import { provideEventsAdminAccess } from '../../core/events-admin-access/events-admin-access.providers';
import { GallerySettingsMockStore } from '../../core/mock/gallery-settings-mock-store';
import { GallerySettingsCard } from './gallery-settings-card';

// The setting comes from the offline mock, whose store holds the server's
// default and range: 12 photos (1–60).
function render(before: (store: GallerySettingsMockStore) => void = () => undefined) {
  TestBed.configureTestingModule({
    imports: [GallerySettingsCard],
    providers: [provideNoopAnimations(), provideEventsAdminAccess(true)],
  });
  const store = TestBed.inject(GallerySettingsMockStore);
  before(store);
  const save = vi.spyOn(TestBed.inject(EVENTS_ADMIN_ACCESS), 'saveGallerySettings');
  const success = vi.spyOn(TestBed.inject(ToastService), 'success');
  const fixture = TestBed.createComponent(GallerySettingsCard);
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

describe('GallerySettingsCard', () => {
  afterEach(() => vi.restoreAllMocks());

  it('shows the cap with its default and range', async () => {
    const { fixture, element } = render((store) => store.save(20));
    await fixture.whenStable();
    expect(element.querySelector('input')?.value).toBe('20');
    expect(text(element.querySelector('mat-hint'))).toBe('Default 12 · allowed 1 to 60');
  });

  it('saves the cap only once it changed, and says what it means', () => {
    const { fixture, card, element, save, success, store } = render();
    expect(button(element, 'Save').disabled).toBe(true);

    card.form.controls.maxPhotos.setValue(1);
    fixture.detectChanges();
    button(element, 'Save').click();
    fixture.detectChanges();

    expect(save).toHaveBeenCalledWith(1);
    expect(store.maxPhotos).toBe(1);
    expect(success).toHaveBeenCalledWith('Saved. A gallery can now hold up to 1 photo.');
    expect(button(element, 'Save').disabled).toBe(true);
  });

  it('fills in the default on reset, for a Save to apply', () => {
    const { fixture, card, element, save } = render((store) => store.save(30));
    button(element, 'Reset to default').click();
    fixture.detectChanges();
    expect(card.form.controls.maxPhotos.value).toBe(12);
    expect(save).not.toHaveBeenCalled();
    button(element, 'Save').click();
    expect(save).toHaveBeenCalledWith(12);
  });

  it('catches an out-of-range, fractional or blank cap before sending', () => {
    const { fixture, card, element, save } = render();
    for (const value of [0, 61, 2.5, null]) {
      card.form.controls.maxPhotos.setValue(value);
      fixture.detectChanges();
      button(element, 'Save').click();
      fixture.detectChanges();
      expect(text(element.querySelector('.gallery-settings__error'))).toBe(
        'Enter a whole number from 1 to 60.',
      );
    }
    expect(save).not.toHaveBeenCalled();
  });

  it('shows the server’s range refusal on the field, and other refusals below', () => {
    const { fixture, card, element, save } = render();
    save.mockReturnValueOnce(
      throwError(() => ({
        status: 400,
        error_code: 'MAX_PHOTOS_OUT_OF_RANGE',
        field: 'event_gallery_max_photos',
        date_index: null,
        detail: 'event_gallery_max_photos is outside its allowed range',
      })),
    );
    card.form.controls.maxPhotos.setValue(40);
    fixture.detectChanges();
    button(element, 'Save').click();
    fixture.detectChanges();
    expect(
      text(
        element.querySelector('[data-field="event_gallery_max_photos"] .gallery-settings__error'),
      ),
    ).toBe('Enter a whole number from 1 to 60.');

    save.mockReturnValueOnce(
      throwError(() => ({
        status: 403,
        error_code: '',
        field: '',
        date_index: null,
        detail: 'Not authorized',
      })),
    );
    card.form.controls.maxPhotos.setValue(41);
    fixture.detectChanges();
    button(element, 'Save').click();
    fixture.detectChanges();
    expect(text(element.querySelector('form > .gallery-settings__error'))).toBe('Not authorized');
  });

  it('says so when the setting can’t be read', () => {
    TestBed.configureTestingModule({
      imports: [GallerySettingsCard],
      providers: [provideNoopAnimations(), provideEventsAdminAccess(true)],
    });
    vi.spyOn(TestBed.inject(EVENTS_ADMIN_ACCESS), 'getGallerySettings').mockReturnValue(
      throwError(() => new Error('offline')),
    );
    const fixture = TestBed.createComponent(GallerySettingsCard);
    fixture.detectChanges();
    expect(text(fixture.nativeElement.querySelector('.gallery-settings__error'))).toBe(
      "The settings couldn't be loaded. Please try again.",
    );
  });
});
