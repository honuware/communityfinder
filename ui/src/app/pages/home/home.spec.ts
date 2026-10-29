import { ComponentFixture, TestBed } from '@angular/core/testing';
import { provideRouter } from '@angular/router';
import { provideNoopAnimations } from '@angular/platform-browser/animations';
import { of } from 'rxjs';
import { COMMUNITY_ACCESS, CommunityAccess } from '../../core/community-access/community-access';
import { normalizeEvent } from '../../core/types/events.types';
import { Home } from './home';

// Only what Home and its upcoming-events strip call.
const mockAccess: Partial<CommunityAccess> = {
  getHealth: () => of({ status: 'ok', version: '1.2.3' }),
  getSiteInfo: () => of({ display_name: 'CommunityFinder', website_url: '', logo_url: '' }),
  getUpcomingEvents: () =>
    of({
      items: [
        normalizeEvent({ id: 7, title: 'Karaoke Night', starts_at_us: 1_786_000_000_000_000 }),
      ],
      from_us: 0,
      to_us: 0,
    }),
};

describe('Home', () => {
  let fixture: ComponentFixture<Home>;

  beforeEach(async () => {
    await TestBed.configureTestingModule({
      imports: [Home],
      providers: [
        provideRouter([]),
        provideNoopAnimations(),
        { provide: COMMUNITY_ACCESS, useValue: mockAccess },
      ],
    }).compileComponents();
    fixture = TestBed.createComponent(Home);
    fixture.detectChanges();
  });

  it('creates', () => {
    expect(fixture.componentInstance).toBeTruthy();
  });

  it('shows the server health from CommunityAccess and the community name', () => {
    const text = fixture.nativeElement.textContent as string;
    expect(text).toContain('ok');
    expect(text).toContain('1.2.3');
    expect(text).toContain('CommunityFinder');
  });

  it('hosts the upcoming-events strip (EV2 §3.9)', () => {
    const strip = (fixture.nativeElement as HTMLElement).querySelector('app-upcoming-events-strip');
    expect(strip?.textContent).toContain('Karaoke Night');
  });
});
