import { provideHttpClient } from '@angular/common/http';
import { HttpTestingController, provideHttpClientTesting } from '@angular/common/http/testing';
import { TestBed } from '@angular/core/testing';
import { firstValueFrom } from 'rxjs';
import { HONUWARE_API_BASE } from '@honuware/ui/access';
import { SubmissionRequest, toSubmissionProblem } from './event-submissions-access';
import { EventSubmissionsHttpAccess, normalizeMySubmission } from './event-submissions-access.http';

function setUp() {
  TestBed.configureTestingModule({
    providers: [
      provideHttpClient(),
      provideHttpClientTesting(),
      { provide: HONUWARE_API_BASE, useValue: '/api' },
      EventSubmissionsHttpAccess,
    ],
  });
  return {
    access: TestBed.inject(EventSubmissionsHttpAccess),
    http: TestBed.inject(HttpTestingController),
  };
}

const REQUEST: SubmissionRequest = {
  title: 'Leather Night',
  dates: [{ starts_at_us: 1_000 }, { starts_at_us: 2_000, ends_at_us: 3_000 }],
  venue_id: 1,
  agreed: true,
};

describe('EventSubmissionsHttpAccess', () => {
  afterEach(() => TestBed.inject(HttpTestingController).verify());

  it('reads whether the form is open', async () => {
    const { access, http } = setUp();
    const open = firstValueFrom(access.getStatus());
    const request = http.expectOne('/api/event_submissions/status');
    expect(request.request.method).toBe('GET');
    expect(request.request.withCredentials).toBe(true);
    request.flush({ accepting_submissions: true });
    expect(await open).toBe(true);

    const paused = firstValueFrom(access.getStatus());
    http.expectOne('/api/event_submissions/status').flush({ accepting_submissions: false });
    expect(await paused).toBe(false);
  });

  it('posts a submission as it is', async () => {
    const { access, http } = setUp();
    const reply = {
      submission_id: 7,
      event_ids: [41, 42],
      ignored_category_slugs: [],
      ignored_scene_tag_slugs: [],
      possible_duplicates: [],
    };
    const result = firstValueFrom(access.submit(REQUEST));
    const request = http.expectOne('/api/event_submissions');
    expect(request.request.method).toBe('POST');
    expect(request.request.withCredentials).toBe(true);
    expect(request.request.body).toEqual(REQUEST);
    request.flush(reply);
    expect(await result).toEqual(reply);
  });

  it('turns a problem body into a SubmissionProblem', async () => {
    const { access, http } = setUp();
    const result = firstValueFrom(access.submit(REQUEST));
    http.expectOne('/api/event_submissions').flush(
      {
        type: 'about:blank',
        title: 'Bad Request',
        status: 400,
        detail: 'Each date must start at least an hour from now.',
        error_code: 'START_TOO_SOON',
        field: 'dates',
        date_index: 1,
      },
      { status: 400, statusText: 'Bad Request' },
    );
    await expect(result).rejects.toEqual({
      status: 400,
      error_code: 'START_TOO_SOON',
      field: 'dates',
      date_index: 1,
      detail: 'Each date must start at least an hour from now.',
    });
  });

  it('reads the pause as a 503 with its code and no field', async () => {
    const { access, http } = setUp();
    const result = firstValueFrom(access.submit(REQUEST));
    http.expectOne('/api/event_submissions').flush(
      {
        status: 503,
        detail: "We're not taking event submissions right now. Please check back soon.",
        error_code: 'SUBMISSIONS_PAUSED',
      },
      { status: 503, statusText: 'Service Unavailable' },
    );
    await expect(result).rejects.toMatchObject({
      status: 503,
      error_code: 'SUBMISSIONS_PAUSED',
      field: '',
      date_index: null,
    });
  });

  it('reads my submissions, newest first as sent', async () => {
    const { access, http } = setUp();
    const result = firstValueFrom(access.getMine());
    const request = http.expectOne('/api/event_submissions/mine');
    expect(request.request.method).toBe('GET');
    expect(request.request.withCredentials).toBe(true);
    request.flush({
      items: [
        {
          id: 9,
          created_us: 5_000,
          relationship: 'organizer',
          regular_thing: true,
          submitter_note: 'Back tables',
          reject_reason: 'not-a-fit',
          message_to_submitter: 'Try the guide instead.',
          dates: [
            {
              event_id: 41,
              title: 'Craft Night',
              starts_at_us: 1_000,
              ends_at_us: null,
              state: 'not_listed',
            },
          ],
        },
        { id: 8, created_us: 4_000, dates: [] },
      ],
    });
    const mine = await result;
    expect(mine.map((submission) => submission.id)).toEqual([9, 8]);
    expect(mine[0].dates[0]).toEqual({
      event_id: 41,
      title: 'Craft Night',
      starts_at_us: 1_000,
      ends_at_us: null,
      state: 'not_listed',
    });
  });

  it('withdraws with an empty body', async () => {
    const { access, http } = setUp();
    const result = firstValueFrom(access.withdraw(9));
    const request = http.expectOne('/api/event_submissions/9/withdraw');
    expect(request.request.method).toBe('POST');
    expect(request.request.withCredentials).toBe(true);
    expect(request.request.body).toEqual({});
    request.flush({ submission_id: 9, withdrawn_event_ids: [41] });
    expect(await result).toEqual({ submission_id: 9, withdrawn_event_ids: [41] });
  });

  it('falls back to a plain message when the error has no problem body', async () => {
    const { access, http } = setUp();
    const result = firstValueFrom(access.getMine());
    http
      .expectOne('/api/event_submissions/mine')
      .flush('Bad gateway', { status: 502, statusText: 'Bad Gateway' });
    await expect(result).rejects.toEqual({
      status: 502,
      error_code: '',
      field: '',
      date_index: null,
      detail: 'Something went wrong. Please try again in a moment.',
    });
  });
});

describe('normalizeMySubmission', () => {
  it('fills in what a submission left out', () => {
    expect(
      normalizeMySubmission({ id: '8', dates: [{ event_id: '41', ends_at_us: 6_000 }] }),
    ).toEqual({
      id: 8,
      created_us: 0,
      relationship: 'other',
      regular_thing: false,
      submitter_note: '',
      reject_reason: '',
      message_to_submitter: '',
      dates: [{ event_id: 41, title: '', starts_at_us: 0, ends_at_us: 6_000, state: '' }],
    });
  });
});

describe('toSubmissionProblem', () => {
  it('passes a problem through as it is', () => {
    const problem = {
      status: 429,
      error_code: 'RATE_LIMITED',
      field: 'max_pending_dates',
      date_index: null,
      detail: 'Wait',
    };
    expect(toSubmissionProblem(problem)).toBe(problem);
  });

  it('reads anything else as a plain failure', () => {
    expect(toSubmissionProblem(new Error('offline'))).toMatchObject({ status: 0, error_code: '' });
  });
});
