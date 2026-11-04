import { TestBed } from '@angular/core/testing';
import { firstValueFrom } from 'rxjs';
import { vi } from 'vitest';
import { CrudAccess } from '@honuware/ui/access';
import { provideHonuwareAccessMock } from '@honuware/ui/testing';
import { HONUWARE_MOCK_OPTIONS } from '../mock/honuware-mock';
import { PastEventSettingsMockStore } from '../mock/past-event-settings-mock-store';
import { dataResultsToRecords } from '../types/events.types';
import { pacificWallClockToUs } from '../utils/events-date.utils';
import { EVENTS_ADMIN_ACCESS } from './events-admin-access';
import { provideEventsAdminAccess } from './events-admin-access.providers';

// Wed Aug 12, 2026, 3:00 PM Pacific: the mock dataset's reference week.
const NOW_US = pacificWallClockToUs(2026, 8, 12, 15);

// In the seed: Open Mic (117) is a curator's pending event; submission #1 has
// two pending dates (201, 202) and #2 one (203).
const OPEN_MIC = 117;

function setUp() {
  let crud!: CrudAccess;
  TestBed.configureTestingModule({
    providers: [
      provideHonuwareAccessMock(HONUWARE_MOCK_OPTIONS, (mocks) => (crud = mocks.crud)),
      provideEventsAdminAccess(true),
    ],
  });
  return { access: TestBed.inject(EVENTS_ADMIN_ACCESS), crud };
}

async function row(crud: CrudAccess, table: string, id: number) {
  return dataResultsToRecords(await firstValueFrom(crud.getRow(table, 'id', String(id))))[0];
}

describe('EventsAdminMockAccess', () => {
  beforeEach(() => {
    vi.useFakeTimers({ toFake: ['Date'] });
    vi.setSystemTime(new Date(NOW_US / 1000));
  });
  afterEach(() => vi.useRealTimers());

  it('decides a curator’s event without emailing anyone', async () => {
    const { access, crud } = setUp();
    const result = await firstValueFrom(
      access.reviewEvent(OPEN_MIC, 'reject', { notes: 'Wrong night', reason: 'other' }),
    );
    expect(result).toEqual({ event_id: OPEN_MIC, status: 'rejected', decision_emailed: false });
    expect(await row(crud, 'events', OPEN_MIC)).toMatchObject({
      status: 'rejected',
      review_notes: 'Wrong night',
    });
  });

  it('emails the submitter once the last date is decided, and only once', async () => {
    const { access, crud } = setUp();
    const first = await firstValueFrom(access.reviewEvent(201, 'approve', { notes: '' }));
    expect(first.decision_emailed).toBe(false);
    expect((await row(crud, 'event_submissions', 1))['decision_notified_us']).toBe('');

    const second = await firstValueFrom(
      access.reviewEvent(202, 'reject', {
        notes: 'Room is booked',
        reason: 'not-a-fit',
        message_to_submitter: 'The second date clashes with a closure.',
      }),
    );
    expect(second.decision_emailed).toBe(true);
    expect(await row(crud, 'event_submissions', 1)).toMatchObject({
      reject_reason: 'not-a-fit',
      message_to_submitter: 'The second date clashes with a closure.',
      decision_notified_us: String(NOW_US),
    });

    // A later change of mind sends nothing more.
    const again = await firstValueFrom(access.reviewEvent(202, 'approve', { notes: '' }));
    expect(again.decision_emailed).toBe(false);
  });

  it('keeps a reason only with a rejection', async () => {
    const { access, crud } = setUp();
    await firstValueFrom(access.reviewEvent(203, 'approve', { notes: '', reason: 'duplicate' }));
    expect((await row(crud, 'event_submissions', 2))['reject_reason']).toBe('');
  });

  it('decides every pending date of a submission in one call', async () => {
    const { access, crud } = setUp();
    const result = await firstValueFrom(
      access.reviewSubmission(1, 'reject', { notes: 'Spam wave', reason: 'spam' }),
    );
    expect(result).toEqual({
      submission_id: 1,
      status: 'rejected',
      decided_event_ids: [201, 202],
      decision_emailed: true,
    });
    for (const id of [201, 202]) {
      expect(await row(crud, 'events', id)).toMatchObject({
        status: 'rejected',
        review_notes: 'Spam wave',
      });
    }
    expect((await row(crud, 'event_submissions', 1))['reject_reason']).toBe('spam');
  });

  it('refuses a missing submission, or one with nothing waiting, as the server does', async () => {
    const { access } = setUp();
    await expect(
      firstValueFrom(access.reviewSubmission(999, 'approve', { notes: '' })),
    ).rejects.toMatchObject({ status: 404, error_code: 'SUBMISSION_NOT_FOUND' });
    await firstValueFrom(access.reviewSubmission(2, 'approve', { notes: '' }));
    await expect(
      firstValueFrom(access.reviewSubmission(2, 'approve', { notes: '' })),
    ).rejects.toMatchObject({ status: 400, error_code: 'NOTHING_PENDING' });
  });

  describe('the submission settings', () => {
    it('starts at the defaults, with the ranges', async () => {
      const { access } = setUp();
      expect(await firstValueFrom(access.getSubmissionSettings())).toEqual({
        max_pending_dates: { value: 10, default: 10, min: 1, max: 100 },
        max_submissions_per_day: { value: 5, default: 5, min: 1, max: 50 },
        submissions_paused: { value: false, default: false },
      });
    });

    it('saves only what it is sent', async () => {
      const { access } = setUp();
      const saved = await firstValueFrom(access.saveSubmissionSettings({ max_pending_dates: 20 }));
      expect(saved.max_pending_dates.value).toBe(20);
      expect(saved.max_submissions_per_day.value).toBe(5);
      const paused = await firstValueFrom(
        access.saveSubmissionSettings({ submissions_paused: true }),
      );
      expect(paused.submissions_paused.value).toBe(true);
      expect(paused.max_pending_dates.value).toBe(20);
    });

    it('refuses an out-of-range limit by name, and saves nothing', async () => {
      const { access } = setUp();
      await expect(
        firstValueFrom(
          access.saveSubmissionSettings({ max_pending_dates: 20, max_submissions_per_day: 0 }),
        ),
      ).rejects.toMatchObject({
        status: 400,
        error_code: 'LIMIT_OUT_OF_RANGE',
        field: 'max_submissions_per_day',
      });
      await expect(
        firstValueFrom(access.saveSubmissionSettings({ max_pending_dates: 2.5 })),
      ).rejects.toMatchObject({ field: 'max_pending_dates' });
      const settings = await firstValueFrom(access.getSubmissionSettings());
      expect(settings.max_pending_dates.value).toBe(10);
      expect(settings.max_submissions_per_day.value).toBe(5);
    });
  });

  describe('the past-event settings', () => {
    it('starts at the defaults and saves only what it is sent', async () => {
      const { access } = setUp();
      expect(await firstValueFrom(access.getPastEventSettings())).toEqual({
        archived_events_visible_days: { value: 365, default: 365, min: 1, max: 730 },
        events_show_past_by_default: { value: false, default: false },
      });
      const saved = await firstValueFrom(
        access.savePastEventSettings({ archived_events_visible_days: 90 }),
      );
      expect(saved.archived_events_visible_days.value).toBe(90);
      expect(saved.events_show_past_by_default.value).toBe(false);
      // The public read sees it.
      expect(TestBed.inject(PastEventSettingsMockStore).publicSettings()).toEqual({
        archived_events_visible_days: 90,
        events_show_past_by_default: false,
      });
    });

    it('refuses an out-of-range window, and saves nothing', async () => {
      const { access } = setUp();
      for (const days of [0, 731, 1.5]) {
        await expect(
          firstValueFrom(
            access.savePastEventSettings({
              archived_events_visible_days: days,
              events_show_past_by_default: true,
            }),
          ),
        ).rejects.toMatchObject({
          status: 400,
          error_code: 'DAYS_OUT_OF_RANGE',
          field: 'archived_events_visible_days',
        });
      }
      const settings = await firstValueFrom(access.getPastEventSettings());
      expect(settings.archived_events_visible_days.value).toBe(365);
      expect(settings.events_show_past_by_default.value).toBe(false);
    });
  });
});
