import { canManageEvents } from './manage-events.guard';

const ADMIN = {
  isAuth: true,
  personId: 1,
  isAdmin: true,
  permissions: ['admin_portal', 'manage_events'],
};
const CURATOR = { isAuth: true, personId: 4, isAdmin: false, permissions: ['manage_events'] };
const GUIDE_CURATOR = { isAuth: true, personId: 5, isAdmin: false, permissions: ['manage_guide'] };
const MEMBER = { isAuth: true, personId: 2, isAdmin: false, permissions: [] };

// Where each kind of person can go is checked through the real routes in
// admin-area.guard.spec.ts, ManageEventsGuard on the Events pages included.
describe('canManageEvents (CM3 Phase 2 §12.5)', () => {
  it('reads an admin, or anyone holding manage_events, as an events curator', () => {
    expect(canManageEvents(ADMIN as never)).toBe(true);
    expect(canManageEvents(CURATOR as never)).toBe(true);
    expect(canManageEvents(GUIDE_CURATOR as never)).toBe(false);
    expect(canManageEvents(MEMBER as never)).toBe(false);
    // An admin whose permissions weren't sent still counts.
    expect(canManageEvents({ isAuth: true, isAdmin: true } as never)).toBe(true);
    expect(canManageEvents({ isAuth: false })).toBe(false);
  });
});
