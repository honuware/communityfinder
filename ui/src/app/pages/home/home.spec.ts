import { ComponentFixture, TestBed } from '@angular/core/testing';
import { provideNoopAnimations } from '@angular/platform-browser/animations';
import { of } from 'rxjs';
import { COMMUNITY_ACCESS, CommunityAccess } from '../../core/community-access/community-access';
import { Home } from './home';

const mockAccess: CommunityAccess = {
  getHealth: () => of({ status: 'ok', version: '1.2.3' }),
  getSiteInfo: () => of({ display_name: 'CommunityFinder', website_url: '', logo_url: '' }),
};

describe('Home', () => {
  let fixture: ComponentFixture<Home>;

  beforeEach(async () => {
    await TestBed.configureTestingModule({
      imports: [Home],
      providers: [provideNoopAnimations(), { provide: COMMUNITY_ACCESS, useValue: mockAccess }],
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
});
