import { TestBed } from '@angular/core/testing';
import { provideRouter } from '@angular/router';
import { RouterTestingHarness } from '@angular/router/testing';
import { provideNoopAnimations } from '@angular/platform-browser/animations';
import { firstValueFrom } from 'rxjs';
import { vi } from 'vitest';
import { CrudAccess } from '@honuware/ui/access';
import { AuthService } from '@honuware/ui/auth';
import { provideHonuwareAccessMock } from '@honuware/ui/testing';
import { provideCommunityAccess } from '../../core/community-access/community-access.providers';
import { provideEventsAdminAccess } from '../../core/events-admin-access/events-admin-access.providers';
import { HONUWARE_MOCK_OPTIONS } from '../../core/mock/honuware-mock';
import { dataResultsToRecords } from '../../core/types/events.types';
import { pacificWallClockToUs } from '../../core/utils/events-date.utils';
import { EMPTY_EVENT_FORM } from './manage-events-form';
import { ManageEventsPage } from './manage-events';

// Wed Aug 12, 2026, 3:00 PM Pacific: the mock dataset's reference week.
const NOW_US = pacificWallClockToUs(2026, 8, 12, 15);

// In the seed: Open Mic is the one pending event; Leather Night (Fri 9 PM –
// Sat 2 AM at The Lantern Room) has category bar (1) and scene leather (2) of
// its own.
const OPEN_MIC = '117';
const LEATHER_NIGHT = '103';
const BAR = 1;
const COMMUNITY = 12;
const BEAR = 1;
const LEATHER = 2;
const ELDERS = 5;

const ADMIN = {
  isAuth: true,
  personId: 1,
  firstName: 'Demo',
  lastName: 'Admin',
  email: 'admin@communityfinder.local',
  createdAt: '0',
  isAdmin: true,
  roles: ['admin'],
  permissions: ['admin_portal', 'manage_events'],
  mustChangePassword: false,
};

// The offline wiring the `local` build uses: the generic CRUD store seeded
// with the events tables, the review mock on top of it, and the vocabularies.
function configure() {
  let crud!: CrudAccess;
  TestBed.configureTestingModule({
    providers: [
      provideRouter([{ path: 'admin/events', component: ManageEventsPage }]),
      provideNoopAnimations(),
      provideHonuwareAccessMock(HONUWARE_MOCK_OPTIONS, (mocks) => (crud = mocks.crud)),
      provideCommunityAccess(true),
      provideEventsAdminAccess(true),
      { provide: AuthService, useValue: { authData: ADMIN } },
    ],
  });
  return crud;
}

async function open(url = '/admin/events') {
  const crud = configure();
  const harness = await RouterTestingHarness.create();
  const page = await harness.navigateByUrl(url, ManageEventsPage);
  return { crud, harness, page, element: harness.routeNativeElement as HTMLElement };
}

async function eventRow(crud: CrudAccess, id: string) {
  return dataResultsToRecords(await firstValueFrom(crud.getRow('events', 'id', id)))[0];
}

async function eventRows(crud: CrudAccess) {
  return dataResultsToRecords(await firstValueFrom(crud.getTableRows('events')));
}

async function assignedTagIds(crud: CrudAccess, table: string, tagColumn: string, eventId: string) {
  const result = await firstValueFrom(
    crud.getFilteredTableRows(table, 'id', true, 500, 0, [
      { column_name: 'event_id', column_value: eventId },
    ]),
  );
  return dataResultsToRecords(result).map((row) => Number(row[tagColumn]));
}

const categoriesOf = (crud: CrudAccess, eventId: string) =>
  assignedTagIds(crud, 'event_category_assignments', 'category_id', eventId);
const sceneTagsOf = (crud: CrudAccess, eventId: string) =>
  assignedTagIds(crud, 'event_scene_tag_assignments', 'scene_tag_id', eventId);

describe('ManageEventsPage', () => {
  beforeEach(() => {
    vi.useFakeTimers({ toFake: ['Date'] });
    vi.setSystemTime(new Date(NOW_US / 1000));
  });
  afterEach(() => {
    vi.useRealTimers();
    vi.restoreAllMocks();
  });

  describe('the review queue', () => {
    it('lists what is waiting, with when and where', async () => {
      const { page, element } = await open();
      expect(page.pending()).toEqual([
        {
          id: 117,
          title: 'Open Mic (awaiting review)',
          summary: 'Thu, Aug 13 · 8:00 PM · Driftwood Hall',
        },
      ]);
      expect(element.textContent).toContain('Waiting for review (1)');
    });

    it('approves an event and takes it off the queue', async () => {
      const { crud, harness, page, element } = await open();
      page.approve(page.pending()[0]);
      harness.detectChanges();
      expect((await eventRow(crud, OPEN_MIC))['status']).toBe('approved');
      expect(page.pending()).toEqual([]);
      expect(element.textContent).toContain('Nothing is waiting for review.');
    });

    it('rejects with a note, and does nothing when the note is cancelled', async () => {
      const prompt = vi.spyOn(window, 'prompt').mockReturnValueOnce(null);
      const { crud, page } = await open();
      page.reject(page.pending()[0]);
      expect((await eventRow(crud, OPEN_MIC))['status']).toBe('pending');
      expect(page.pending().length).toBe(1);

      prompt.mockReturnValueOnce('Duplicate of the Friday listing');
      page.reject(page.pending()[0]);
      const row = await eventRow(crud, OPEN_MIC);
      expect(row['status']).toBe('rejected');
      expect(row['review_notes']).toBe('Duplicate of the Friday listing');
      expect(page.pending()).toEqual([]);
    });
  });

  describe('the form', () => {
    it('opens ?edit=<id> in Seattle time, with the event’s own tags checked', async () => {
      const { page, element } = await open(`/admin/events?edit=${LEATHER_NIGHT}`);
      expect(page.editingId()).toBe(LEATHER_NIGHT);
      expect(page.form.getRawValue()).toEqual({
        ...EMPTY_EVENT_FORM,
        title: 'Leather Night',
        description: 'Dress code encouraged, not enforced. Coat check at the door.',
        startDate: '2026-08-14',
        startTime: '21:00',
        endDate: '2026-08-15',
        endTime: '02:00',
        venueId: '1',
        costText: '$10 cover',
      });
      expect(page.selectedCategoryIds()).toEqual([BAR]);
      expect(page.selectedSceneTagIds()).toEqual([LEATHER]);
      expect(element.textContent).toContain('Editing event 103');
    });

    it('adds a new event as pending, with its tags, and puts it in the queue', async () => {
      const { crud, page } = await open();
      const add = vi.spyOn(crud, 'addItemFetchPrimaryKey');
      page.newEvent();
      page.form.setValue({
        ...EMPTY_EVENT_FORM,
        title: 'Elders Bingo',
        startDate: '2026-09-04',
        startTime: '19:00',
        endTime: '21:00',
        venueId: '3',
        costText: 'Free',
      });
      page.toggleCategory(COMMUNITY, true);
      page.toggleSceneTag(ELDERS, true);
      page.save();

      const added = (await eventRows(crud)).find((row) => row['title'] === 'Elders Bingo')!;
      expect(added).toMatchObject({
        starts_at_us: String(pacificWallClockToUs(2026, 9, 4, 19)),
        ends_at_us: String(pacificWallClockToUs(2026, 9, 4, 21)),
        venue_id: '3',
        cost_text: 'Free',
        status: 'pending',
        created_by_person_id: '1',
      });
      // A blank number column is left out of the insert ('' isn't a number).
      expect(add.mock.calls[0][0].value).not.toHaveProperty('series_id');
      expect(await categoriesOf(crud, added['id'])).toEqual([COMMUNITY]);
      expect(await sceneTagsOf(crud, added['id'])).toEqual([ELDERS]);
      // The form stays on the saved event, now in the queue for approval.
      expect(page.editingId()).toBe(added['id']);
      expect(page.pending().map((item) => item.title)).toContain('Elders Bingo');
    });

    it('writes no tag rows for a series event left unchecked, so it keeps the series’', async () => {
      const { crud, page } = await open();
      page.newEvent();
      page.form.setValue({
        ...EMPTY_EVENT_FORM,
        title: 'Bear Happy Hour',
        startDate: '2026-09-10',
        startTime: '18:00',
        venueId: '1',
        seriesId: '11',
      });
      page.save();
      const added = (await eventRows(crud)).find(
        (row) => row['title'] === 'Bear Happy Hour' && row['status'] === 'pending',
      )!;
      expect(added['series_id']).toBe('11');
      expect(await categoriesOf(crud, added['id'])).toEqual([]);
      expect(await sceneTagsOf(crud, added['id'])).toEqual([]);
    });

    it('saves only what changed, and swaps the tags', async () => {
      const { crud, page } = await open(`/admin/events?edit=${LEATHER_NIGHT}`);
      const update = vi.spyOn(crud, 'updateItem');
      page.form.controls.costText.setValue('$15 cover');
      page.toggleSceneTag(LEATHER, false);
      page.toggleSceneTag(BEAR, true);
      page.save();

      expect(update).toHaveBeenCalledTimes(1);
      expect(update.mock.calls[0][0]).toEqual({
        table_name: 'events',
        column_name: 'id',
        column_value: LEATHER_NIGHT,
        value: { cost_text: '$15 cover' },
      });
      expect((await eventRow(crud, LEATHER_NIGHT))['title']).toBe('Leather Night');
      expect(await categoriesOf(crud, LEATHER_NIGHT)).toEqual([BAR]);
      expect(await sceneTagsOf(crud, LEATHER_NIGHT)).toEqual([BEAR]);
    });

    it('empties a text field', async () => {
      const { crud, page } = await open(`/admin/events?edit=${LEATHER_NIGHT}`);
      const update = vi.spyOn(crud, 'updateItem');
      page.form.controls.description.setValue('');
      page.save();
      expect(update.mock.calls[0][0].value).toEqual({ description: '' });
      expect((await eventRow(crud, LEATHER_NIGHT))['description']).toBe('');
      expect(page.formError()).toBe('');
    });

    it('asks for a start before saving', async () => {
      const { crud, page } = await open();
      const add = vi.spyOn(crud, 'addItemFetchPrimaryKey');
      page.form.controls.title.setValue('No date yet');
      page.save();
      expect(page.formError()).toBe('Fill in the title and the start date and time.');
      expect(add).not.toHaveBeenCalled();
    });
  });
});
