import { TestBed } from '@angular/core/testing';
import { provideRouter } from '@angular/router';
import { provideNoopAnimations } from '@angular/platform-browser/animations';
import { provideHttpClient } from '@angular/common/http';
import { HONUWARE_API_BASE } from '@honuware/ui/access';
import { provideHonuwareAccessMock } from '@honuware/ui/testing';
import { App } from './app';

// The shell renders the header (community name + auth chip) + footer + router-outlet.
// The header now injects AuthService + PhotoUrlBuilder, so the mock access stack is
// wired in; SiteConfigService is providedIn root and falls back to its default when
// no community access is loaded, so the header shows the default community name.
describe('App', () => {
  beforeEach(async () => {
    await TestBed.configureTestingModule({
      imports: [App],
      providers: [
        provideRouter([]),
        provideNoopAnimations(),
        provideHttpClient(),
        { provide: HONUWARE_API_BASE, useValue: '/api' },
        ...provideHonuwareAccessMock(),
      ],
    }).compileComponents();
  });

  it('creates the app', () => {
    const fixture = TestBed.createComponent(App);
    expect(fixture.componentInstance).toBeTruthy();
  });

  it('renders the community name in the shell', () => {
    const fixture = TestBed.createComponent(App);
    fixture.detectChanges();
    const text = fixture.nativeElement.textContent as string;
    expect(text).toContain('CommunityFinder');
  });
});
