import { provideHttpClient } from '@angular/common/http';
import { HttpTestingController, provideHttpClientTesting } from '@angular/common/http/testing';
import { TestBed } from '@angular/core/testing';
import { firstValueFrom } from 'rxjs';
import { HONUWARE_API_BASE } from '@honuware/ui/access';
import { PastEventSettings, SubmissionSettings } from './events-admin-access';
import { EventsAdminHttpAccess } from './events-admin-access.http';

function setUp() {
  TestBed.configureTestingModule({
    providers: [
      provideHttpClient(),
      provideHttpClientTesting(),
      { provide: HONUWARE_API_BASE, useValue: '/api' },
      EventsAdminHttpAccess,
    ],
  });
  return {
    access: TestBed.inject(EventsAdminHttpAccess),
    http: TestBed.inject(HttpTestingController),
  };
}

const SETTINGS: SubmissionSettings = {
  max_pending_dates: { value: 10, default: 10, min: 1, max: 100 },
  max_submissions_per_day: { value: 5, default: 5, min: 1, max: 50 },
  submissions_paused: { value: false, default: false },
};

describe('EventsAdminHttpAccess', () => {
  afterEach(() => TestBed.inject(HttpTestingController).verify());

  it('reviews one event with the decision in the body', async () => {
    const { access, http } = setUp();
    const result = firstValueFrom(
      access.reviewEvent(203, 'reject', {
        notes: 'Same night as #103',
        reason: 'duplicate',
        message_to_submitter: 'It’s already listed.',
      }),
    );
    const request = http.expectOne('/api/admin/review_event/203');
    expect(request.request.method).toBe('POST');
    expect(request.request.withCredentials).toBe(true);
    expect(request.request.body).toEqual({
      action: 'reject',
      notes: 'Same night as #103',
      reason: 'duplicate',
      message_to_submitter: 'It’s already listed.',
    });
    request.flush({ event_id: 203, status: 'rejected', decision_emailed: true });
    expect(await result).toEqual({ event_id: 203, status: 'rejected', decision_emailed: true });
  });

  it('approves with only the action and the note', async () => {
    const { access, http } = setUp();
    const result = firstValueFrom(access.reviewEvent(117, 'approve', { notes: '' }));
    const request = http.expectOne('/api/admin/review_event/117');
    expect(request.request.body).toEqual({ action: 'approve', notes: '' });
    request.flush({ event_id: 117, status: 'approved', decision_emailed: false });
    expect((await result).decision_emailed).toBe(false);
  });

  it('reviews a whole submission in one call', async () => {
    const { access, http } = setUp();
    const result = firstValueFrom(access.reviewSubmission(1, 'approve', { notes: '' }));
    const request = http.expectOne('/api/admin/review_submission/1');
    expect(request.request.method).toBe('POST');
    expect(request.request.withCredentials).toBe(true);
    expect(request.request.body).toEqual({ action: 'approve', notes: '' });
    request.flush({
      submission_id: 1,
      status: 'approved',
      decided_event_ids: [201, 202],
      decision_emailed: true,
    });
    expect((await result).decided_event_ids).toEqual([201, 202]);
  });

  it('reads a submission review’s refusal as a problem', async () => {
    const { access, http } = setUp();
    const result = firstValueFrom(access.reviewSubmission(1, 'reject', { notes: '' }));
    http.expectOne('/api/admin/review_submission/1').flush(
      {
        status: 400,
        detail: 'Nothing in this submission is waiting for review',
        error_code: 'NOTHING_PENDING',
      },
      { status: 400, statusText: 'Bad Request' },
    );
    await expect(result).rejects.toMatchObject({ status: 400, error_code: 'NOTHING_PENDING' });
  });

  it('reads and saves the submission settings', async () => {
    const { access, http } = setUp();
    const loaded = firstValueFrom(access.getSubmissionSettings());
    const get = http.expectOne('/api/admin/event_submission_settings');
    expect(get.request.method).toBe('GET');
    expect(get.request.withCredentials).toBe(true);
    get.flush(SETTINGS);
    expect(await loaded).toEqual(SETTINGS);

    const saved = firstValueFrom(access.saveSubmissionSettings({ max_pending_dates: 20 }));
    const put = http.expectOne('/api/admin/event_submission_settings');
    expect(put.request.method).toBe('PUT');
    expect(put.request.withCredentials).toBe(true);
    expect(put.request.body).toEqual({ max_pending_dates: 20 });
    put.flush({ ...SETTINGS, max_pending_dates: { ...SETTINGS.max_pending_dates, value: 20 } });
    expect((await saved).max_pending_dates.value).toBe(20);
  });

  it('names the field of an out-of-range limit', async () => {
    const { access, http } = setUp();
    const saved = firstValueFrom(access.saveSubmissionSettings({ max_submissions_per_day: 500 }));
    http.expectOne('/api/admin/event_submission_settings').flush(
      {
        status: 400,
        detail: 'max_submissions_per_day must be from 1 to 50',
        error_code: 'LIMIT_OUT_OF_RANGE',
        field: 'max_submissions_per_day',
      },
      { status: 400, statusText: 'Bad Request' },
    );
    await expect(saved).rejects.toMatchObject({
      error_code: 'LIMIT_OUT_OF_RANGE',
      field: 'max_submissions_per_day',
    });
  });

  it('reads and saves the past-event settings, naming the field of a refusal', async () => {
    const { access, http } = setUp();
    const settings: PastEventSettings = {
      archived_events_visible_days: { value: 365, default: 365, min: 1, max: 730 },
      events_show_past_by_default: { value: false, default: false },
    };
    const loaded = firstValueFrom(access.getPastEventSettings());
    const get = http.expectOne('/api/admin/past_event_settings');
    expect(get.request.method).toBe('GET');
    expect(get.request.withCredentials).toBe(true);
    get.flush(settings);
    expect(await loaded).toEqual(settings);

    const saved = firstValueFrom(
      access.savePastEventSettings({ events_show_past_by_default: true }),
    );
    const put = http.expectOne('/api/admin/past_event_settings');
    expect(put.request.method).toBe('PUT');
    expect(put.request.body).toEqual({ events_show_past_by_default: true });
    put.flush({ ...settings, events_show_past_by_default: { value: true, default: false } });
    expect((await saved).events_show_past_by_default.value).toBe(true);

    const refused = firstValueFrom(
      access.savePastEventSettings({ archived_events_visible_days: 0 }),
    );
    http.expectOne('/api/admin/past_event_settings').flush(
      {
        status: 400,
        detail: 'archived_events_visible_days is outside its allowed range',
        error_code: 'DAYS_OUT_OF_RANGE',
        field: 'archived_events_visible_days',
      },
      { status: 400, statusText: 'Bad Request' },
    );
    await expect(refused).rejects.toMatchObject({
      error_code: 'DAYS_OUT_OF_RANGE',
      field: 'archived_events_visible_days',
    });
  });
});
