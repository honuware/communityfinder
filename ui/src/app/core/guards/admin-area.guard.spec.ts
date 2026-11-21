import { Component } from '@angular/core';
import { TestBed } from '@angular/core/testing';
import { provideNoopAnimations } from '@angular/platform-browser/animations';
import { provideRouter, Route, Router } from '@angular/router';
import { RouterTestingHarness } from '@angular/router/testing';
import { of } from 'rxjs';
import { AuthService } from '@honuware/ui/auth';
import { routes as appRoutes } from '../../app.routes';
import { canManageGuide, canUseAdminArea } from './admin-area.guard';

@Component({ template: '' })
class Blank {}

const ADMIN = {
  isAuth: true,
  personId: 1,
  isAdmin: true,
  permissions: ['admin_portal', 'manage_events', 'manage_guide'],
};
const EVENTS_CURATOR = {
  isAuth: true,
  personId: 4,
  isAdmin: false,
  permissions: ['manage_events'],
};
const GUIDE_CURATOR = { isAuth: true, personId: 5, isAdmin: false, permissions: ['manage_guide'] };
const MEMBER = { isAuth: true, personId: 2, isAdmin: false, permissions: [] };

// The app's own /admin entry, its guards and its lazy children (with their own
// guards), with the pages it lands on swapped for a blank one so nothing loads
// data; where a visit ends up is what's checked.
async function visit(url: string, authData: object): Promise<string> {
  const admin = appRoutes.find((route) => route.path === 'admin')!;
  const children = ((await import('../../pages/admin/admin.routes')).default as Route[]).map(
    (route) => ({ ...route, component: Blank, children: undefined }),
  );
  TestBed.configureTestingModule({
    providers: [
      provideRouter([
        { path: '', component: Blank },
        { path: 'login', component: Blank },
        { path: 'admin', canActivate: admin.canActivate, children },
      ]),
      provideNoopAnimations(),
      { provide: AuthService, useValue: { authData, authData$: of(authData) } },
    ],
  });
  const harness = await RouterTestingHarness.create();
  await harness.navigateByUrl(url);
  return TestBed.inject(Router).url;
}

async function expectVisits(urls: string[], authData: object, landing?: string): Promise<void> {
  for (const url of urls) {
    TestBed.resetTestingModule();
    expect(await visit(url, authData), url).toBe(landing ?? url);
  }
}

describe('AdminAreaGuard (CM3 Phase 2 §12.5, OQ24)', () => {
  it('lets an events curator into the dashboard, the Events pages and Manage Data', async () => {
    await expectVisits(
      ['/admin', '/admin/events', '/admin/events/settings', '/admin/tables'],
      EVENTS_CURATOR,
    );
  });

  it('keeps an events curator out of Users, Roles and Site settings', async () => {
    await expectVisits(['/admin/users', '/admin/roles', '/admin/settings'], EVENTS_CURATOR, '/');
  });

  it('lets a guide curator into the dashboard and Manage Data', async () => {
    await expectVisits(['/admin', '/admin/tables'], GUIDE_CURATOR);
  });

  it('keeps a guide curator out of the Events pages and the admin-only pages', async () => {
    await expectVisits(
      [
        '/admin/events',
        '/admin/events/settings',
        '/admin/users',
        '/admin/roles',
        '/admin/settings',
      ],
      GUIDE_CURATOR,
      '/',
    );
  });

  it('lets someone holding both into the Events pages and Manage Data', async () => {
    const both = { ...EVENTS_CURATOR, permissions: ['manage_events', 'manage_guide'] };
    await expectVisits(['/admin', '/admin/events', '/admin/tables'], both);
    await expectVisits(['/admin/users'], both, '/');
  });

  it('lets an admin in everywhere', async () => {
    await expectVisits(
      [
        '/admin',
        '/admin/users',
        '/admin/roles',
        '/admin/events',
        '/admin/events/settings',
        '/admin/tables',
        '/admin/settings',
      ],
      ADMIN,
    );
  });

  it('sends a member home, and a signed-out visitor to sign in', async () => {
    await expectVisits(['/admin', '/admin/tables'], MEMBER, '/');
    TestBed.resetTestingModule();
    expect(await visit('/admin', { isAuth: false })).toBe('/login?returnUrl=%2Fadmin');
  });

  it('reads an admin, or anyone holding manage_guide, as a guide curator', () => {
    expect(canManageGuide(ADMIN as never)).toBe(true);
    expect(canManageGuide(GUIDE_CURATOR as never)).toBe(true);
    expect(canManageGuide(EVENTS_CURATOR as never)).toBe(false);
    // An admin whose permissions weren't sent still counts.
    expect(canManageGuide({ isAuth: true, isAdmin: true } as never)).toBe(true);
    expect(canManageGuide({ isAuth: false })).toBe(false);
  });

  it('opens the admin area to admins and either kind of curator', () => {
    expect(canUseAdminArea(ADMIN as never)).toBe(true);
    expect(canUseAdminArea(EVENTS_CURATOR as never)).toBe(true);
    expect(canUseAdminArea(GUIDE_CURATOR as never)).toBe(true);
    expect(canUseAdminArea(MEMBER as never)).toBe(false);
    expect(canUseAdminArea({ isAuth: false })).toBe(false);
  });
});
