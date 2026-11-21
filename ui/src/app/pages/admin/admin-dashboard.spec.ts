import { TestBed } from '@angular/core/testing';
import { provideRouter } from '@angular/router';
import { provideNoopAnimations } from '@angular/platform-browser/animations';
import { of } from 'rxjs';
import { AuthService } from '@honuware/ui/auth';
import { AdminDashboard } from './admin-dashboard';

const ADMIN = {
  isAuth: true,
  personId: 1,
  isAdmin: true,
  permissions: ['admin_portal', 'manage_events'],
};
// The Curator role (CM3 Phase 2 §12.1): manage_events and nothing else.
const CURATOR = { isAuth: true, personId: 4, isAdmin: false, permissions: ['manage_events'] };
// CM3 OQ24: someone who curates only the guide.
const GUIDE_CURATOR = { isAuth: true, personId: 5, isAdmin: false, permissions: ['manage_guide'] };

function render(authData: object = ADMIN) {
  TestBed.configureTestingModule({
    imports: [AdminDashboard],
    providers: [
      provideRouter([]),
      provideNoopAnimations(),
      { provide: AuthService, useValue: { authData, authData$: of(authData) } },
    ],
  });
  const fixture = TestBed.createComponent(AdminDashboard);
  fixture.detectChanges();
  const element = fixture.nativeElement as HTMLElement;
  return {
    element,
    hrefs: () => Array.from(element.querySelectorAll('a')).map((a) => a.getAttribute('href')),
  };
}

describe('AdminDashboard', () => {
  it('renders the Roles & Permissions tile linking to the bespoke role page', () => {
    const { element, hrefs } = render();
    expect(element.textContent ?? '').toContain('Roles & Permissions');
    expect(hrefs()).toContain('/admin/roles');
  });

  it('renders the Events tile linking to the event editor', () => {
    const { element, hrefs } = render();
    expect(element.textContent ?? '').toContain('Review, add, and edit events');
    expect(hrefs()).toContain('/admin/events');
  });

  it('gives an admin every tile', () => {
    const { hrefs } = render();
    expect(hrefs()).toEqual([
      '/admin/roles',
      '/admin/users',
      '/admin/events',
      '/admin/tables',
      '/admin/settings',
    ]);
  });

  it('tells an admin that Manage Data holds every table', () => {
    const { element } = render();
    expect(element.textContent ?? '').toContain('Browse and edit every admin table');
  });

  // CM3 Phase 2 §12.5.
  it('gives a curator only the tiles they can use', () => {
    const { element, hrefs } = render(CURATOR);
    expect(hrefs()).toEqual(['/admin/events', '/admin/tables']);
    expect(element.textContent ?? '').toContain('Browse and edit the events tables');
    expect(element.textContent ?? '').not.toContain('Roles & Permissions');
  });

  // CM3 OQ24.
  it('gives a guide curator Manage Data alone, holding the guide tables', () => {
    const { element, hrefs } = render(GUIDE_CURATOR);
    expect(hrefs()).toEqual(['/admin/tables']);
    expect(element.textContent ?? '').toContain('Browse and edit the guide tables');
    expect(element.textContent ?? '').not.toContain('Review, add, and edit events');
  });

  it('gives someone curating both the Events tile and both kinds of table', () => {
    const { element, hrefs } = render({
      ...CURATOR,
      permissions: ['manage_events', 'manage_guide'],
    });
    expect(hrefs()).toEqual(['/admin/events', '/admin/tables']);
    expect(element.textContent ?? '').toContain('Browse and edit the events and guide tables');
  });
});
