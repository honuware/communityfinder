import { inject, Injectable } from '@angular/core';
import { concat, forkJoin, last, map, Observable, of, switchMap, throwError } from 'rxjs';
import { FilterPair, HONUWARE_CRUD_ACCESS, HONUWARE_PHOTO_ACCESS } from '@honuware/ui/access';
import { AuthService } from '@honuware/ui/auth';
import { buildEventsMockData } from '../mock/events-mock-data';
import { SubmissionSettingsMockStore } from '../mock/submission-settings-mock-store';
import { dataResultsToRecords, US_PER_DAY } from '../types/events.types';
import { isPossibleDuplicate } from '../utils/duplicate-hints';
import { nowUs } from '../utils/events-date.utils';
import { parseVideoLink, VIDEO_ERROR_HOST_NOT_SUPPORTED } from '../utils/video-links';
import {
  EventSubmissionsAccess,
  FlyerImageType,
  FlyerResult,
  MySubmission,
  MySubmissionDate,
  PossibleDuplicate,
  SubmissionDateState,
  SubmissionProblem,
  SubmissionRelationship,
  SubmissionRequest,
  SubmissionResult,
  WithdrawResult,
} from './event-submissions-access';

const HOUR_US = 3_600 * 1_000_000;
const MAX_DATES = 12;
const WITHDRAWN_NOTE = 'withdrawn by the submitter';

// Offline mock — the `local` configuration wires this so `ng serve` runs with no
// backend. Submissions live in the same in-memory CRUD store the review queue
// reads (event_submissions, plus pending `events` rows), so a form sent
// offline shows up in /admin/events, and a decision there shows up on "My
// submissions". It runs the server's main checks with the server's messages
// (the pause switch, the agreement, the title, the dates, the venue, the link
// and both rate limits, the last two from the shared SubmissionSettingsMockStore),
// and fakes the duplicate hints from the store's approved events by the
// server's two rules. A flyer goes into the offline photo store on every
// waiting date (CM3), so "My submissions" and the curator's editor see it.
@Injectable()
export class EventSubmissionsMockAccess implements EventSubmissionsAccess {
  private crud = inject(HONUWARE_CRUD_ACCESS);
  private photos = inject(HONUWARE_PHOTO_ACCESS);
  private auth = inject(AuthService);
  private settings = inject(SubmissionSettingsMockStore);
  // The vocabularies' ids, as the CRUD seed numbered them.
  private readonly vocabulary = buildEventsMockData(nowUs());

  getStatus(): Observable<boolean> {
    return of(!this.settings.paused);
  }

  submit(request: SubmissionRequest): Observable<SubmissionResult> {
    const personId = this.personId();
    if (personId === null) {
      return throwError(() => problem(401, '', '', 'Sign in to submit an event'));
    }
    if (this.settings.paused) {
      return throwError(() =>
        problem(
          503,
          'SUBMISSIONS_PAUSED',
          '',
          "We're not taking event submissions right now. Please check back soon.",
        ),
      );
    }
    if (request.website) {
      return of(emptyResult());
    }
    const refused = checkForm(request, nowUs());
    if (refused) {
      return throwError(() => refused);
    }
    return forkJoin({
      venue: request.venue_id
        ? this.rows('venues', [{ column_name: 'id', column_value: String(request.venue_id) }])
        : of([{}]),
      mine: this.rows('event_submissions', [
        { column_name: 'submitted_by_person_id', column_value: personId },
      ]),
      events: this.rows('events', []),
    }).pipe(
      switchMap(({ venue, mine, events }) => {
        if (venue.length === 0) {
          return throwError(() =>
            problem(400, 'VENUE_NOT_FOUND', 'venue_id', "That venue isn't in our list."),
          );
        }
        const limited = this.checkLimits(request, mine, events, nowUs());
        return limited ? throwError(() => limited) : this.write(request, personId, events);
      }),
    );
  }

  getMine(): Observable<MySubmission[]> {
    const personId = this.personId();
    if (personId === null) {
      return throwError(() => problem(401, '', '', 'Sign in to see your submissions'));
    }
    return forkJoin({
      submissions: this.rows('event_submissions', [
        { column_name: 'submitted_by_person_id', column_value: personId },
      ]),
      events: this.rows('events', []),
    }).pipe(
      switchMap(({ submissions, events }) => {
        const ids = new Set(submissions.map((row) => row['id']));
        const theirs = events.filter((event) => ids.has(event['submission_id']));
        const photoChecks = theirs.map((event) =>
          this.photos
            .hasPhoto('events', Number(event['id']))
            .pipe(map((reply) => (reply.has_photo ? event['id'] : ''))),
        );
        return (photoChecks.length ? forkJoin(photoChecks) : of([] as string[])).pipe(
          map((withPhoto) => {
            const photoIds = new Set(withPhoto.filter(Boolean));
            return submissions
              .map((row) => toMySubmission(row, events, photoIds))
              .sort((a, b) => b.created_us - a.created_us || b.id - a.id);
          }),
        );
      }),
    );
  }

  attachFlyer(
    submissionId: number,
    imageData: ArrayBuffer,
    imageType: FlyerImageType,
  ): Observable<FlyerResult> {
    const personId = this.personId();
    if (personId === null) {
      return throwError(() => problem(401, '', '', 'Sign in to add a flyer'));
    }
    return this.waitingDatesOf(submissionId, personId).pipe(
      switchMap((pending) => {
        if (imageType !== 'jpeg' && imageType !== 'png') {
          return throwError(() =>
            problem(400, 'BAD_IMAGE_TYPE', '', 'The flyer must be a JPEG or PNG image.'),
          );
        }
        if (imageData.byteLength === 0) {
          return throwError(() => problem(400, 'BAD_IMAGE', '', 'The flyer is empty.'));
        }
        return concat(
          ...pending.map((event) =>
            this.photos.uploadPhoto('events', Number(event['id']), imageData, imageType),
          ),
        ).pipe(
          last(),
          map(() => ({
            submission_id: submissionId,
            event_ids: pending.map((event) => Number(event['id'])),
          })),
        );
      }),
    );
  }

  // As the server's RemoveFlyer: the photo comes off each waiting date that
  // has one, and only those are named. A listed date keeps its photo.
  removeFlyer(submissionId: number): Observable<FlyerResult> {
    const personId = this.personId();
    if (personId === null) {
      return throwError(() => problem(401, '', '', 'Sign in to remove a flyer'));
    }
    return this.waitingDatesOf(submissionId, personId).pipe(
      switchMap((pending) =>
        forkJoin(
          pending.map((event) => {
            const eventId = Number(event['id']);
            return this.photos
              .hasPhoto('events', eventId)
              .pipe(
                switchMap((reply) =>
                  reply.has_photo
                    ? this.photos.deletePhoto('events', eventId).pipe(map(() => eventId))
                    : of(0),
                ),
              );
          }),
        ),
      ),
      map((removed) => ({
        submission_id: submissionId,
        event_ids: removed.filter((eventId) => eventId !== 0),
      })),
    );
  }

  withdraw(submissionId: number): Observable<WithdrawResult> {
    const personId = this.personId();
    if (personId === null) {
      return throwError(() => problem(401, '', '', 'Sign in to withdraw a submission'));
    }
    const id = String(submissionId);
    return forkJoin({
      submission: this.rows('event_submissions', [{ column_name: 'id', column_value: id }]),
      pending: this.rows('events', [
        { column_name: 'submission_id', column_value: id },
        { column_name: 'status', column_value: 'pending' },
      ]),
    }).pipe(
      switchMap(({ submission, pending }) => {
        const row = submission[0];
        if (!row || row['submitted_by_person_id'] !== personId) {
          return throwError(() => problem(404, '', '', 'Submission not found'));
        }
        if (pending.length === 0) {
          return throwError(() =>
            problem(400, 'NOTHING_PENDING', '', 'Nothing in this submission is waiting for review'),
          );
        }
        const writes = [
          ...pending.map((event) =>
            this.crud.updateItem({
              table_name: 'events',
              column_name: 'id',
              column_value: event['id'],
              value: { status: 'rejected', review_notes: WITHDRAWN_NOTE },
            }),
          ),
          this.crud.updateItem({
            table_name: 'event_submissions',
            column_name: 'id',
            column_value: id,
            value: {
              reject_reason: row['reject_reason'] || 'withdrawn',
              decision_notified_us: row['decision_notified_us'] || String(nowUs()),
              updated_us: String(nowUs()),
            },
          }),
        ];
        return concat(...writes).pipe(
          last(),
          map(() => ({
            submission_id: submissionId,
            withdrawn_event_ids: pending.map((event) => Number(event['id'])),
          })),
        );
      }),
    );
  }

  private personId(): string | null {
    const authData = this.auth.authData;
    return authData.isAuth ? String(authData.personId) : null;
  }

  // The flyer routes' checks, as the server makes them: someone else's
  // submission is a 404, and one with nothing waiting is NOTHING_PENDING.
  // Otherwise its waiting dates.
  private waitingDatesOf(
    submissionId: number,
    personId: string,
  ): Observable<Record<string, string>[]> {
    const id = String(submissionId);
    return forkJoin({
      submission: this.rows('event_submissions', [{ column_name: 'id', column_value: id }]),
      pending: this.rows('events', [
        { column_name: 'submission_id', column_value: id },
        { column_name: 'status', column_value: 'pending' },
      ]),
    }).pipe(
      switchMap(({ submission, pending }) => {
        const row = submission[0];
        if (!row || row['submitted_by_person_id'] !== personId) {
          return throwError(() => problem(404, '', '', 'Submission not found'));
        }
        if (pending.length === 0) {
          return throwError(() =>
            problem(
              400,
              'NOTHING_PENDING',
              '',
              'Nothing in this submission is waiting for review.',
            ),
          );
        }
        return of(pending);
      }),
    );
  }

  private checkLimits(
    request: SubmissionRequest,
    mine: Record<string, string>[],
    events: Record<string, string>[],
    now: number,
  ): SubmissionProblem | null {
    const today = mine.filter((row) => Number(row['created_us']) >= now - US_PER_DAY).length;
    if (today >= this.settings.maxSubmissionsPerDay) {
      return problem(
        429,
        'RATE_LIMITED',
        'max_submissions_per_day',
        `You can send up to ${count(this.settings.maxSubmissionsPerDay, 'submission', 'submissions')} in 24 hours. Please try again later.`,
      );
    }
    const mySubmissionIds = new Set(mine.map((row) => row['id']));
    const pending = events.filter(
      (row) => row['status'] === 'pending' && mySubmissionIds.has(row['submission_id']),
    ).length;
    const adding = request.dates.length;
    if (pending + adding > this.settings.maxPendingDates) {
      return problem(
        429,
        'RATE_LIMITED',
        'max_pending_dates',
        `You can have up to ${count(this.settings.maxPendingDates, 'date', 'dates')} waiting for review at a time. ` +
          `You have ${pending} waiting, and this would add ${adding}. We'll get to them soon.`,
      );
    }
    return null;
  }

  private write(
    request: SubmissionRequest,
    personId: string,
    events: Record<string, string>[],
  ): Observable<SubmissionResult> {
    const now = String(nowUs());
    const categoryIds = new Map(this.vocabulary.categories.map((entry) => [entry.slug, entry.id]));
    const sceneTagIds = new Map(this.vocabulary.sceneTags.map((entry) => [entry.slug, entry.id]));
    const categorySlugs = request.category_slugs ?? [];
    const sceneTagSlugs = request.scene_tag_slugs ?? [];
    // checkForm has read it already; stored canonical, as on the server.
    const video = parseVideoLink(request.video_url ?? '');
    const videoUrl = video.ok ? video.canonicalUrl : '';

    return this.crud
      .addItemFetchPrimaryKey({
        table_name: 'event_submissions',
        value: {
          submitted_by_person_id: personId,
          submitter_note: request.submitter_note ?? '',
          relationship: request.relationship ?? 'other',
          regular_thing: request.regular_thing ? 't' : 'f',
          reject_reason: '',
          message_to_submitter: '',
          decision_notified_us: '',
          created_us: now,
          updated_us: now,
        },
      })
      .pipe(
        switchMap((submissionId) =>
          forkJoin(
            request.dates.map((date) =>
              this.crud.addItemFetchPrimaryKey({
                table_name: 'events',
                value: {
                  title: request.title.trim(),
                  description: request.description ?? '',
                  starts_at_us: String(date.starts_at_us),
                  ends_at_us: date.ends_at_us === undefined ? '' : String(date.ends_at_us),
                  venue_id: request.venue_id ? String(request.venue_id) : '',
                  location_text: request.location_text ?? '',
                  url: request.url ?? '',
                  cost_text: request.cost_text ?? '',
                  video_url: videoUrl,
                  series_id: '',
                  origin: 'user_submitted',
                  status: 'pending',
                  created_by_person_id: personId,
                  review_notes: '',
                  submission_id: submissionId,
                },
              }),
            ),
          ).pipe(map((eventIds) => ({ submissionId, eventIds }))),
        ),
        switchMap(({ submissionId, eventIds }) => {
          const tagWrites = eventIds.flatMap((eventId) => [
            ...categorySlugs
              .filter((slug) => categoryIds.has(slug))
              .map((slug) =>
                this.crud.addItem({
                  table_name: 'event_category_assignments',
                  value: { event_id: eventId, category_id: String(categoryIds.get(slug)) },
                }),
              ),
            ...sceneTagSlugs
              .filter((slug) => sceneTagIds.has(slug))
              .map((slug) =>
                this.crud.addItem({
                  table_name: 'event_scene_tag_assignments',
                  value: { event_id: eventId, scene_tag_id: String(sceneTagIds.get(slug)) },
                }),
              ),
          ]);
          const result: SubmissionResult = {
            submission_id: Number(submissionId),
            event_ids: eventIds.map(Number),
            ignored_category_slugs: categorySlugs.filter((slug) => !categoryIds.has(slug)),
            ignored_scene_tag_slugs: sceneTagSlugs.filter((slug) => !sceneTagIds.has(slug)),
            possible_duplicates: duplicateHints(request, events),
          };
          return tagWrites.length
            ? concat(...tagWrites).pipe(
                last(),
                map(() => result),
              )
            : of(result);
        }),
      );
  }

  private rows(table: string, filters: FilterPair[]): Observable<Record<string, string>[]> {
    return this.crud.getRowByValues(table, filters).pipe(map(dataResultsToRecords));
  }
}

function problem(
  status: number,
  errorCode: string,
  field: string,
  detail: string,
  dateIndex: number | null = null,
): SubmissionProblem {
  return { status, error_code: errorCode, field, date_index: dateIndex, detail };
}

function count(value: number, singular: string, plural: string): string {
  return `${value} ${value === 1 ? singular : plural}`;
}

function emptyResult(): SubmissionResult {
  return {
    submission_id: 0,
    event_ids: [],
    ignored_category_slugs: [],
    ignored_scene_tag_slugs: [],
    possible_duplicates: [],
  };
}

// The server's field checks, in its order, with its messages.
function checkForm(request: SubmissionRequest, now: number): SubmissionProblem | null {
  if (!request.agreed) {
    return problem(
      400,
      'NOT_AGREED',
      'agreed',
      'Please confirm the details are accurate and can be listed publicly.',
    );
  }
  const titleLength = [...request.title.trim()].length;
  if (titleLength < 3 || titleLength > 120) {
    return problem(400, 'TITLE_LENGTH', 'title', 'The title must be 3 to 120 characters.');
  }
  if (request.dates.length === 0) {
    return problem(400, 'NO_DATES', 'dates', 'Add at least one date.');
  }
  if (request.dates.length > MAX_DATES) {
    return problem(400, 'TOO_MANY_DATES', 'dates', 'A submission can have up to 12 dates.');
  }
  const starts = new Set<number>();
  for (const [index, date] of request.dates.entries()) {
    const refuse = (code: string, detail: string) => problem(400, code, 'dates', detail, index);
    if (date.starts_at_us < now + HOUR_US) {
      return refuse('START_TOO_SOON', 'Each date must start at least an hour from now.');
    }
    if (date.starts_at_us > now + 365 * US_PER_DAY) {
      return refuse('START_TOO_FAR', 'Each date must be within the next year.');
    }
    if (date.ends_at_us !== undefined && date.ends_at_us <= date.starts_at_us) {
      return refuse('END_BEFORE_START', 'The end must come after the start.');
    }
    if (date.ends_at_us !== undefined && date.ends_at_us - date.starts_at_us > 4 * US_PER_DAY) {
      return refuse('TOO_LONG', 'A date can last at most 4 days.');
    }
    if (starts.has(date.starts_at_us)) {
      return refuse('DUPLICATE_DATE', 'This date is listed twice.');
    }
    starts.add(date.starts_at_us);
  }
  const url = (request.url ?? '').trim();
  if (url && !/^https?:\/\/\S+$/i.test(url)) {
    return problem(400, 'BAD_URL', 'url', 'Links must start with http:// or https://.');
  }
  if ((request.video_url ?? '').trim()) {
    const video = parseVideoLink(request.video_url ?? '');
    if (!video.ok) {
      return problem(
        400,
        video.errorCode,
        'video_url',
        video.errorCode === VIDEO_ERROR_HOST_NOT_SUPPORTED
          ? 'Video links can be from YouTube or Vimeo.'
          : "That isn't a link to one YouTube or Vimeo video.",
      );
    }
  }
  return null;
}

// Approved events only, as on the server: another person's pending
// submission isn't the submitter's to see.
function duplicateHints(
  request: SubmissionRequest,
  events: Record<string, string>[],
): PossibleDuplicate[] {
  const approved = events
    .filter((row) => row['status'] === 'approved')
    .map((row) => ({
      id: Number(row['id']),
      title: row['title'],
      startsAtUs: Number(row['starts_at_us']),
      venueId: row['venue_id'] ? Number(row['venue_id']) : null,
    }));
  return request.dates.flatMap((date, index) => {
    const submitted = {
      id: 0,
      title: request.title,
      startsAtUs: date.starts_at_us,
      venueId: request.venue_id ?? null,
    };
    return approved
      .filter((candidate) => isPossibleDuplicate(submitted, candidate))
      .map((candidate) => ({
        date_index: index,
        event_id: candidate.id,
        title: candidate.title,
        starts_at_us: candidate.startsAtUs,
      }));
  });
}

function dateState(row: Record<string, string>): SubmissionDateState {
  switch (row['status']) {
    case 'pending':
      return 'waiting';
    case 'approved':
      return 'listed';
    case 'archived':
      return 'past';
    default:
      return row['review_notes'] === WITHDRAWN_NOTE ? 'withdrawn' : 'not_listed';
  }
}

function toMySubmission(
  row: Record<string, string>,
  events: Record<string, string>[],
  eventIdsWithPhoto: ReadonlySet<string>,
): MySubmission {
  const dates: MySubmissionDate[] = events
    .filter((event) => event['submission_id'] === row['id'])
    .map((event) => {
      const video = parseVideoLink(event['video_url'] ?? '');
      return {
        event_id: Number(event['id']),
        title: event['title'],
        starts_at_us: Number(event['starts_at_us']),
        ends_at_us: event['ends_at_us'] ? Number(event['ends_at_us']) : null,
        state: dateState(event),
        has_photo: eventIdsWithPhoto.has(event['id']),
        video: video.ok ? video.video : null,
      };
    })
    .sort((a, b) => a.starts_at_us - b.starts_at_us || a.event_id - b.event_id);
  return {
    id: Number(row['id']),
    created_us: Number(row['created_us']),
    relationship: (row['relationship'] || 'other') as SubmissionRelationship,
    regular_thing: row['regular_thing'] === 't',
    submitter_note: row['submitter_note'] ?? '',
    reject_reason: row['reject_reason'] ?? '',
    message_to_submitter: row['message_to_submitter'] ?? '',
    dates,
  };
}
