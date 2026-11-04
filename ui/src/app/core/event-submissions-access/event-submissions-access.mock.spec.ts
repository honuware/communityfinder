import { TestBed } from '@angular/core/testing';
import { firstValueFrom } from 'rxjs';
import { vi } from 'vitest';
import { CrudAccess, FilterPair } from '@honuware/ui/access';
import { AuthService } from '@honuware/ui/auth';
import { provideHonuwareAccessMock } from '@honuware/ui/testing';
import { HONUWARE_MOCK_OPTIONS } from '../mock/honuware-mock';
import { SubmissionSettingsMockStore } from '../mock/submission-settings-mock-store';
import { dataResultsToRecords } from '../types/events.types';
import { pacificWallClockToUs } from '../utils/events-date.utils';
import { EVENT_SUBMISSIONS_ACCESS, SubmissionRequest } from './event-submissions-access';
import { provideEventSubmissionsAccess } from './event-submissions-access.providers';

// Wed Aug 12, 2026, 3:00 PM Pacific: the mock dataset's reference week.
const NOW_US = pacificWallClockToUs(2026, 8, 12, 15);
const HOUR_US = 3_600 * 1_000_000;

// The listed Leather Night starts Fri Aug 14 at 9 PM at venue 1.
const LEATHER_NIGHT = 103;
const LEATHER_NIGHT_STARTS_US = pacificWallClockToUs(2026, 8, 14, 21);
// The seed's pending "Leather Night!" (Grace's submission #2), an hour later.
const GRACES_LEATHER_NIGHT = 203;

const DEMO_ADMIN = 1;
const ADA = 2;
const GRACE = 3;

const REQUEST: SubmissionRequest = {
  title: 'Elders Bingo',
  dates: [{ starts_at_us: pacificWallClockToUs(2026, 9, 4, 19) }],
  venue_id: 3,
  agreed: true,
};

function setUp(personId: number | null = DEMO_ADMIN) {
  let crud!: CrudAccess;
  const auth = {
    authData: personId === null ? { isAuth: false } : { isAuth: true, personId },
  };
  TestBed.configureTestingModule({
    providers: [
      provideHonuwareAccessMock(HONUWARE_MOCK_OPTIONS, (mocks) => (crud = mocks.crud)),
      provideEventSubmissionsAccess(true),
      { provide: AuthService, useValue: auth },
    ],
  });
  return {
    access: TestBed.inject(EVENT_SUBMISSIONS_ACCESS),
    settings: TestBed.inject(SubmissionSettingsMockStore),
    crud,
    signInAs: (id: number) => (auth.authData = { isAuth: true, personId: id }),
  };
}

async function rows(crud: CrudAccess, table: string, filters: FilterPair[]) {
  return dataResultsToRecords(await firstValueFrom(crud.getRowByValues(table, filters)));
}

describe('EventSubmissionsMockAccess', () => {
  beforeEach(() => {
    vi.useFakeTimers({ toFake: ['Date'] });
    vi.setSystemTime(new Date(NOW_US / 1000));
  });
  afterEach(() => vi.useRealTimers());

  describe('submit', () => {
    it('writes the submission and its pending dates into the store the queue reads', async () => {
      const { access, crud } = setUp();
      const second = pacificWallClockToUs(2026, 9, 11, 19);
      const result = await firstValueFrom(
        access.submit({
          ...REQUEST,
          dates: [...REQUEST.dates, { starts_at_us: second, ends_at_us: second + 2 * HOUR_US }],
          description: 'Cards provided.',
          category_slugs: ['community', 'not-a-category'],
          scene_tag_slugs: ['elders'],
          relationship: 'organizer',
          regular_thing: true,
          submitter_note: 'Every other Friday.',
        }),
      );
      expect(result.ignored_category_slugs).toEqual(['not-a-category']);
      expect(result.ignored_scene_tag_slugs).toEqual([]);
      expect(result.possible_duplicates).toEqual([]);
      expect(result.event_ids.length).toBe(2);

      const [submission] = await rows(crud, 'event_submissions', [
        { column_name: 'id', column_value: String(result.submission_id) },
      ]);
      expect(submission).toMatchObject({
        submitted_by_person_id: String(DEMO_ADMIN),
        relationship: 'organizer',
        regular_thing: 't',
        submitter_note: 'Every other Friday.',
        created_us: String(NOW_US),
      });
      const events = await rows(crud, 'events', [
        { column_name: 'submission_id', column_value: String(result.submission_id) },
      ]);
      expect(events.map((event) => Number(event['id'])).sort()).toEqual(
        [...result.event_ids].sort(),
      );
      for (const event of events) {
        expect(event).toMatchObject({
          title: 'Elders Bingo',
          venue_id: '3',
          origin: 'user_submitted',
          status: 'pending',
          created_by_person_id: String(DEMO_ADMIN),
          description: 'Cards provided.',
        });
      }
      const categories = await rows(crud, 'event_category_assignments', [
        { column_name: 'event_id', column_value: String(result.event_ids[0]) },
      ]);
      expect(categories.length).toBe(1);
      const sceneTags = await rows(crud, 'event_scene_tag_assignments', [
        { column_name: 'event_id', column_value: String(result.event_ids[1]) },
      ]);
      expect(sceneTags.length).toBe(1);
    });

    it('hints at listed events that look the same, never at pending ones', async () => {
      const { access } = setUp();
      const result = await firstValueFrom(
        access.submit({
          ...REQUEST,
          title: 'LEATHER NIGHT',
          venue_id: 1,
          dates: [
            { starts_at_us: pacificWallClockToUs(2026, 9, 4, 19) },
            { starts_at_us: LEATHER_NIGHT_STARTS_US + HOUR_US },
          ],
        }),
      );
      expect(result.possible_duplicates).toEqual([
        {
          date_index: 1,
          event_id: LEATHER_NIGHT,
          title: 'Leather Night',
          starts_at_us: LEATHER_NIGHT_STARTS_US,
        },
      ]);
      expect(result.possible_duplicates.map((hint) => hint.event_id)).not.toContain(
        GRACES_LEATHER_NIGHT,
      );
    });

    it('refuses a signed-out visitor', async () => {
      const { access } = setUp(null);
      await expect(firstValueFrom(access.submit(REQUEST))).rejects.toMatchObject({ status: 401 });
    });

    it('refuses with a 503 while paused', async () => {
      const { access, settings } = setUp();
      settings.save({ submissions_paused: true });
      expect(await firstValueFrom(access.getStatus())).toBe(false);
      await expect(firstValueFrom(access.submit(REQUEST))).rejects.toEqual({
        status: 503,
        error_code: 'SUBMISSIONS_PAUSED',
        field: '',
        date_index: null,
        detail: "We're not taking event submissions right now. Please check back soon.",
      });
      settings.save({ submissions_paused: false });
      expect(await firstValueFrom(access.getStatus())).toBe(true);
    });

    it('answers a filled-in honeypot as if it worked, and writes nothing', async () => {
      const { access, crud } = setUp();
      const before = (await rows(crud, 'event_submissions', [])).length;
      const result = await firstValueFrom(
        access.submit({ ...REQUEST, website: 'https://spam.example' }),
      );
      expect(result.submission_id).toBe(0);
      expect(result.event_ids).toEqual([]);
      expect((await rows(crud, 'event_submissions', [])).length).toBe(before);
    });

    it('runs the server’s field checks, naming the field and the date', async () => {
      const { access } = setUp();
      const refusal = (request: SubmissionRequest) => firstValueFrom(access.submit(request));
      await expect(refusal({ ...REQUEST, agreed: false })).rejects.toMatchObject({
        error_code: 'NOT_AGREED',
        field: 'agreed',
      });
      await expect(refusal({ ...REQUEST, title: 'Hi' })).rejects.toMatchObject({
        error_code: 'TITLE_LENGTH',
        field: 'title',
        detail: 'The title must be 3 to 120 characters.',
      });
      await expect(
        refusal({ ...REQUEST, dates: [...REQUEST.dates, { starts_at_us: NOW_US + HOUR_US / 2 }] }),
      ).rejects.toMatchObject({
        status: 400,
        error_code: 'START_TOO_SOON',
        field: 'dates',
        date_index: 1,
      });
      await expect(
        refusal({ ...REQUEST, dates: [REQUEST.dates[0], REQUEST.dates[0]] }),
      ).rejects.toMatchObject({ error_code: 'DUPLICATE_DATE', date_index: 1 });
      await expect(refusal({ ...REQUEST, url: 'example.org' })).rejects.toMatchObject({
        error_code: 'BAD_URL',
        field: 'url',
      });
      await expect(refusal({ ...REQUEST, venue_id: 999 })).rejects.toMatchObject({
        error_code: 'VENUE_NOT_FOUND',
        field: 'venue_id',
      });
    });

    it('holds a person to the submissions-per-day limit', async () => {
      const { access, settings } = setUp();
      settings.save({ max_submissions_per_day: 1 });
      await firstValueFrom(access.submit(REQUEST));
      await expect(firstValueFrom(access.submit(REQUEST))).rejects.toEqual({
        status: 429,
        error_code: 'RATE_LIMITED',
        field: 'max_submissions_per_day',
        date_index: null,
        detail: 'You can send up to 1 submission in 24 hours. Please try again later.',
      });
    });

    it('holds a person to the pending-dates limit', async () => {
      const { access, settings } = setUp(ADA);
      // Ada already has two dates waiting (submission #1).
      settings.save({ max_pending_dates: 2 });
      await expect(firstValueFrom(access.submit(REQUEST))).rejects.toEqual({
        status: 429,
        error_code: 'RATE_LIMITED',
        field: 'max_pending_dates',
        date_index: null,
        detail:
          'You can have up to 2 dates waiting for review at a time. ' +
          "You have 2 waiting, and this would add 1. We'll get to them soon.",
      });
    });
  });

  describe('getMine', () => {
    it('lists only my submissions, newest first, each date’s state', async () => {
      const { access, crud } = setUp(ADA);
      const sent = await firstValueFrom(access.submit(REQUEST));
      await firstValueFrom(
        crud.updateItem({
          table_name: 'events',
          column_name: 'id',
          column_value: '201',
          value: { status: 'approved' },
        }),
      );
      await firstValueFrom(
        crud.updateItem({
          table_name: 'events',
          column_name: 'id',
          column_value: '202',
          value: { status: 'rejected', review_notes: 'Not a fit' },
        }),
      );
      const mine = await firstValueFrom(access.getMine());
      expect(mine.map((submission) => submission.id)).toEqual([sent.submission_id, 1]);
      expect(mine[1]).toMatchObject({
        relationship: 'organizer',
        regular_thing: true,
        submitter_note: 'We meet every other week at the back tables.',
      });
      expect(mine[1].dates.map((date) => [date.event_id, date.state])).toEqual([
        [201, 'listed'],
        [202, 'not_listed'],
      ]);
      expect(mine[0].dates.map((date) => date.state)).toEqual(['waiting']);
    });

    it('reads an archived date as past', async () => {
      const { access, crud } = setUp(GRACE);
      await firstValueFrom(
        crud.updateItem({
          table_name: 'events',
          column_name: 'id',
          column_value: String(GRACES_LEATHER_NIGHT),
          value: { status: 'archived' },
        }),
      );
      const [submission] = await firstValueFrom(access.getMine());
      expect(submission.dates.map((date) => date.state)).toEqual(['past']);
    });
  });

  describe('withdraw', () => {
    it('takes my waiting dates out of the queue', async () => {
      const { access, crud } = setUp(ADA);
      const result = await firstValueFrom(access.withdraw(1));
      expect(result.submission_id).toBe(1);
      expect([...result.withdrawn_event_ids].sort()).toEqual([201, 202]);

      const events = await rows(crud, 'events', [
        { column_name: 'submission_id', column_value: '1' },
      ]);
      expect(events.map((event) => [event['status'], event['review_notes']])).toEqual([
        ['rejected', 'withdrawn by the submitter'],
        ['rejected', 'withdrawn by the submitter'],
      ]);
      const [submission] = await rows(crud, 'event_submissions', [
        { column_name: 'id', column_value: '1' },
      ]);
      expect(submission['reject_reason']).toBe('withdrawn');
      expect(submission['decision_notified_us']).toBe(String(NOW_US));

      const [mine] = await firstValueFrom(access.getMine());
      expect(mine.dates.map((date) => date.state)).toEqual(['withdrawn', 'withdrawn']);

      await expect(firstValueFrom(access.withdraw(1))).rejects.toMatchObject({
        status: 400,
        error_code: 'NOTHING_PENDING',
      });
    });

    it('can’t touch someone else’s submission', async () => {
      const { access, signInAs } = setUp(GRACE);
      await expect(firstValueFrom(access.withdraw(1))).rejects.toMatchObject({ status: 404 });
      signInAs(ADA);
      await expect(firstValueFrom(access.withdraw(999))).rejects.toMatchObject({ status: 404 });
    });
  });
});
