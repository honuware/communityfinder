import { TestBed } from '@angular/core/testing';
import { provideNoopAnimations } from '@angular/platform-browser/animations';
import { provideRouter } from '@angular/router';
import { HonuwareAccessMocks, provideHonuwareAccessMock } from '@honuware/ui/testing';
import { SiteSettingsPage } from './site-settings';

const MB = 1024 * 1024;

// CM3 Phase 2 §13.3 (OQ21). The card itself is @honuware/ui's, with its own
// specs; this is the page holding it.
describe('SiteSettingsPage', () => {
  function render() {
    let mocks: HonuwareAccessMocks | undefined;
    TestBed.configureTestingModule({
      imports: [SiteSettingsPage],
      providers: [
        provideRouter([]),
        provideNoopAnimations(),
        provideHonuwareAccessMock({}, (captured) => (mocks = captured)),
      ],
    });
    const fixture = TestBed.createComponent(SiteSettingsPage);
    fixture.detectChanges();
    return { fixture, element: fixture.nativeElement as HTMLElement, mocks: mocks! };
  }

  it('holds the upload-limit card, showing the limit', () => {
    const { element } = render();
    expect(element.querySelector('h1')?.textContent?.trim()).toBe('Site settings');
    expect(element.querySelector('hw-image-upload-settings')).not.toBeNull();
    expect(element.querySelector<HTMLInputElement>('hw-image-upload-settings input')?.value).toBe(
      '10',
    );
  });

  it('saves a new limit through the card', () => {
    const { fixture, element, mocks } = render();
    const input = element.querySelector<HTMLInputElement>('hw-image-upload-settings input')!;
    input.value = '20';
    input.dispatchEvent(new Event('input'));
    fixture.detectChanges();
    element.querySelector('hw-image-upload-settings form')!.dispatchEvent(new Event('submit'));
    fixture.detectChanges();
    expect(mocks.imageUploadSettings.maxUploadBytes).toBe(20 * MB);
  });

  it('leads back to the admin dashboard', () => {
    const { element } = render();
    expect(element.querySelector('a.admin-back')?.getAttribute('href')).toBe('/admin');
  });
});
