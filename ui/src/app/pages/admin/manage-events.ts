import { NgTemplateOutlet } from '@angular/common';
import {
  Component,
  computed,
  DestroyRef,
  ElementRef,
  inject,
  OnInit,
  signal,
  viewChild,
} from '@angular/core';
import { takeUntilDestroyed } from '@angular/core/rxjs-interop';
import { FormControl, FormGroup, ReactiveFormsModule } from '@angular/forms';
import { ActivatedRoute, RouterLink } from '@angular/router';
import {
  catchError,
  debounceTime,
  distinctUntilChanged,
  filter,
  forkJoin,
  map,
  Observable,
  of,
  switchMap,
} from 'rxjs';
import {
  MatAutocompleteModule,
  MatAutocompleteSelectedEvent,
} from '@angular/material/autocomplete';
import { MatButtonModule } from '@angular/material/button';
import { MatFormFieldModule } from '@angular/material/form-field';
import { MatIconModule } from '@angular/material/icon';
import { MatInputModule } from '@angular/material/input';
import { MatProgressSpinnerModule } from '@angular/material/progress-spinner';
import { MatSelectModule } from '@angular/material/select';
import { MatTabsModule } from '@angular/material/tabs';
import { FkOption, FkOptionsResponse, HONUWARE_CRUD_ACCESS } from '@honuware/ui/access';
import { ToastService } from '@honuware/ui/foundation';
import {
  EVENTS_ADMIN_ACCESS,
  REJECT_REASONS,
  RejectReason,
  ReviewAction,
  ReviewDecision,
} from '../../core/events-admin-access/events-admin-access';
import { EventVocabulariesService } from '../../core/services/event-vocabularies.service';
import { dataResultsToRecords } from '../../core/types/events.types';
import { DuplicateCandidate, isPossibleDuplicate } from '../../core/utils/duplicate-hints';
import { eventPagePath } from '../../core/utils/event-links';
import { formatEventShortDate, formatEventTimeRange } from '../../core/utils/events-date.utils';
import { relationshipLabel } from '../../core/utils/submission-labels';
import { EditRequest, EventEditor, SavedEvent, TagChoice } from './event-editor';

// Enough rows for the whole pending queue. Also how many of the latest
// approved events the duplicate flag compares against.
const PAGE_SIZE = 500;

// The tabs under the queue.
export const NEW_EVENT_TAB = 0;
export const EDIT_EVENT_TAB = 1;

// An event the queue's duplicate flag points at.
interface DuplicateOf {
  id: number;
  title: string;
}

export interface PendingEvent {
  id: number;
  title: string;
  // "Fri, Aug 14 · 10:00 PM – 2:00 AM"
  when: string;
  // The venue's name, or the place as typed; '' for neither.
  where: string;
  description: string;
  url: string;
  costText: string;
  // The submission it came in on; null for one a curator or a feed entered.
  submissionId: number | null;
  // Submitted events only: listed or pending events it looks like (EV3's two
  // duplicate rules), and whether it came with no link to check it by.
  possibleDuplicates: DuplicateOf[];
  noLink: boolean;
}

// What the queue shows about a submission, above its dates.
export interface SubmissionSummary {
  id: number;
  // "Ada Lovelace", or "member #2" when the name can't be read.
  submitter: string;
  relationship: string;
  note: string;
  regularThing: boolean;
}

// One card in the queue: a submission's dates together, or one event on its
// own (a curator's or a feed's).
export interface QueueGroup {
  key: string;
  submission: SubmissionSummary | null;
  events: PendingEvent[];
  // Its dates' flags together, for the card's top line.
  possibleDuplicates: DuplicateOf[];
  noLink: boolean;
}

// What the reject panel is open for.
export type RejectTarget =
  { kind: 'event'; event: PendingEvent } | { kind: 'submission'; group: QueueGroup };

// The events page for curators (/admin/events). Like ManageRolesPage and
// ManageUsersPage, it talks to the generic HONUWARE_CRUD_ACCESS directly,
// plus the bespoke review endpoints.
//
// - The pending queue, one card per event or submission: its title, its date,
//   any flags (a possible duplicate, a missing link), and Approve / Reject.
//   "Details" opens the rest: who sent it, their note and relationship, the
//   place, cost, link and description, and each of a submission's dates
//   with its own buttons. A submission's dates are approved or rejected
//   together with Approve all / Reject all (EV3 §5.5). Rejecting opens a
//   panel in the card for a curator note, and for a submitted event, a reason
//   and a message to the submitter.
// - A banner while submissions are paused, so the switch isn't left on.
// - Two tabs below: New Event, and Edit Event, which finds an event by title,
//   or opens one from the queue's Edit or from an event page's "Edit" link
//   (?edit=<id>). Each tab has its own EventEditor, so switching tabs keeps
//   what was typed in the other. A newly added event opens in Edit Event.
@Component({
  selector: 'app-manage-events',
  imports: [
    NgTemplateOutlet,
    RouterLink,
    ReactiveFormsModule,
    MatAutocompleteModule,
    MatButtonModule,
    MatFormFieldModule,
    MatIconModule,
    MatInputModule,
    MatProgressSpinnerModule,
    MatSelectModule,
    MatTabsModule,
    EventEditor,
  ],
  templateUrl: './manage-events.html',
  styleUrl: './manage-events.scss',
})
export class ManageEventsPage implements OnInit {
  private crud = inject(HONUWARE_CRUD_ACCESS);
  private eventsAdmin = inject(EVENTS_ADMIN_ACCESS);
  private vocabularyService = inject(EventVocabulariesService);
  private toast = inject(ToastService);
  private route = inject(ActivatedRoute);
  private destroyRef = inject(DestroyRef);

  private readonly editorSection = viewChild<ElementRef<HTMLElement>>('editorSection');

  // The pending events' rows, as the generic table read sends them.
  private readonly pendingRows = signal<Record<string, string>[]>([]);
  // For the queue's submitted events: their submissions by id, the
  // submitters' names by person id, and the latest approved events to check
  // for duplicates against.
  private readonly submissionRows = signal<Record<string, Record<string, string>>>({});
  private readonly submitterNames = signal<Record<string, string>>({});
  private readonly approvedRows = signal<Record<string, string>[]>([]);
  readonly loadingQueue = signal(true);
  // The event, or the submission ("s<id>"), being reviewed.
  readonly reviewingKey = signal<string | null>(null);
  readonly submissionsPaused = signal(false);

  readonly rejecting = signal<RejectTarget | null>(null);
  readonly rejectForm = new FormGroup({
    reason: new FormControl<RejectReason | ''>('', { nonNullable: true }),
    notes: new FormControl('', { nonNullable: true }),
    message: new FormControl('', { nonNullable: true }),
  });

  // A getter, not a field: the unit-test runner's module transform misses an
  // import used as a bare class-field initializer (see SubmitEventPage).
  get rejectReasons(): typeof REJECT_REASONS {
    return REJECT_REASONS;
  }

  readonly venues = signal<FkOption[]>([]);
  readonly pending = computed(() => {
    const rows = this.pendingRows();
    const candidates = [...rows, ...this.approvedRows()].map(toCandidate);
    return rows.map((row) => this.toPendingEvent(row, candidates));
  });
  readonly queue = computed(() => this.groupQueue(this.pending()));
  // The cards whose details are open, by queue key. All start closed.
  readonly expandedKeys = signal<string[]>([]);

  readonly series = signal<FkOption[]>([]);
  readonly categories = signal<TagChoice[]>([]);
  readonly sceneTags = signal<TagChoice[]>([]);

  readonly selectedTab = signal(NEW_EVENT_TAB);
  // What the Edit Event tab has open; null until an event is picked.
  readonly editRequest = signal<EditRequest | null>(null);
  readonly searchControl = new FormControl<string | FkOption>('', { nonNullable: true });
  readonly searchOptions = signal<FkOption[]>([]);

  ngOnInit(): void {
    this.loadQueue();
    this.loadPickers();
    this.eventsAdmin
      .getSubmissionSettings()
      .pipe(takeUntilDestroyed(this.destroyRef))
      .subscribe({
        next: (settings) => this.submissionsPaused.set(settings.submissions_paused.value),
        error: () => this.submissionsPaused.set(false),
      });
    this.vocabularyService.vocabularies$
      .pipe(takeUntilDestroyed(this.destroyRef))
      .subscribe(({ categories, sceneTags }) => {
        this.categories.set(categories.map((tag) => ({ id: tag.id, label: tag.display_name })));
        this.sceneTags.set(sceneTags.map((tag) => ({ id: tag.id, label: tag.display_name })));
      });

    // Find an event by title. Picking an option emits the option itself
    // (not a string), which opens it instead of searching again.
    this.searchControl.valueChanges
      .pipe(
        filter((value): value is string => typeof value === 'string'),
        map((value) => value.trim()),
        debounceTime(200),
        distinctUntilChanged(),
        switchMap((text) =>
          text
            ? this.crud.getFkOptions('events', text, 10)
            : of<FkOptionsResponse>({ total_count: 0, options: [] }),
        ),
        takeUntilDestroyed(this.destroyRef),
      )
      .subscribe((response) => this.searchOptions.set(response.options));

    this.route.queryParamMap.pipe(takeUntilDestroyed(this.destroyRef)).subscribe((params) => {
      const id = params.get('edit');
      if (id) {
        this.editEvent(id);
      }
    });
  }

  displayOption(option: FkOption | string): string {
    return typeof option === 'string' ? option : (option?.display ?? '');
  }

  onSearchSelected(event: MatAutocompleteSelectedEvent): void {
    this.editEvent((event.option.value as FkOption).value);
    this.searchControl.setValue('', { emitEvent: false });
  }

  // Opens the event in the Edit Event tab.
  editEvent(id: string): void {
    this.editRequest.set({ id });
    this.selectedTab.set(EDIT_EVENT_TAB);
  }

  // A queue card's Edit: the tabs are below the queue, so bring them into view.
  editFromQueue(item: PendingEvent): void {
    this.editEvent(String(item.id));
    // Guarded: not every DOM has it (the unit tests' doesn't).
    this.editorSection()?.nativeElement.scrollIntoView?.({ behavior: 'smooth', block: 'start' });
  }

  // A save in either tab: the queue may have changed, and a newly added event
  // opens in Edit Event (its New Event form is blank again).
  onSaved(saved: SavedEvent): void {
    this.loadQueue();
    if (saved.added) {
      this.editEvent(saved.id);
    }
  }

  isExpanded(key: string): boolean {
    return this.expandedKeys().includes(key);
  }

  toggleDetails(key: string): void {
    this.expandedKeys.update((keys) =>
      keys.includes(key) ? keys.filter((other) => other !== key) : [...keys, key],
    );
  }

  // The key a card's own Approve and Reject go by: its one event's, or the
  // whole submission's.
  headerKey(group: QueueGroup): string {
    return group.events.length === 1 ? this.eventKey(group.events[0]) : group.key;
  }

  // ", and 1 more date" after a submission's first date; '' for one date.
  moreDates(group: QueueGroup): string {
    const more = group.events.length - 1;
    return more > 0 ? `, and ${more} more ${more === 1 ? 'date' : 'dates'}` : '';
  }

  approve(item: PendingEvent): void {
    this.review(item, 'approve', { notes: '' });
  }

  // Opens the reject panel under the event.
  reject(item: PendingEvent): void {
    this.openRejectPanel({ kind: 'event', event: item });
  }

  // Every pending date of the submission, in one call.
  approveAll(group: QueueGroup): void {
    this.reviewGroup(group, 'approve', { notes: '' });
  }

  // Opens the reject panel under the submission.
  rejectAll(group: QueueGroup): void {
    this.openRejectPanel({ kind: 'submission', group });
  }

  isRejecting(key: string): boolean {
    const target = this.rejecting();
    return !!target && targetKey(target) === key;
  }

  eventKey(item: PendingEvent): string {
    return `e${item.id}`;
  }

  groupKey(group: QueueGroup): string {
    return group.key;
  }

  // The reason and the message are for submitted events only.
  rejectingSubmitted(): boolean {
    const target = this.rejecting();
    return !!target && (target.kind === 'submission' || target.event.submissionId !== null);
  }

  cancelReject(): void {
    this.rejecting.set(null);
  }

  confirmReject(): void {
    const target = this.rejecting();
    if (!target) {
      return;
    }
    const { reason, notes, message } = this.rejectForm.getRawValue();
    const decision: ReviewDecision = { notes: notes.trim() };
    if (this.rejectingSubmitted()) {
      if (reason) {
        decision.reason = reason;
      }
      if (message.trim()) {
        decision.message_to_submitter = message.trim();
      }
    }
    this.rejecting.set(null);
    if (target.kind === 'event') {
      this.review(target.event, 'reject', decision);
    } else {
      this.reviewGroup(target.group, 'reject', decision);
    }
  }

  previewPath(id: number | string): string {
    return eventPagePath(Number(id));
  }

  private openRejectPanel(target: RejectTarget): void {
    this.rejectForm.reset();
    this.rejecting.set(target);
  }

  private review(item: PendingEvent, action: ReviewAction, decision: ReviewDecision): void {
    this.reviewingKey.set(this.eventKey(item));
    this.eventsAdmin
      .reviewEvent(item.id, action, decision)
      .pipe(takeUntilDestroyed(this.destroyRef))
      .subscribe({
        next: (result) => {
          this.reviewingKey.set(null);
          this.removeFromQueue([item.id]);
          this.toast.success(
            (action === 'approve' ? `Approved "${item.title}".` : `Rejected "${item.title}".`) +
              emailedNote(result.decision_emailed),
          );
        },
        error: () => {
          this.reviewingKey.set(null);
          this.toast.error(`Couldn't ${action} "${item.title}".`);
        },
      });
  }

  private reviewGroup(group: QueueGroup, action: ReviewAction, decision: ReviewDecision): void {
    const submission = group.submission;
    if (!submission) {
      return;
    }
    const title = group.events[0]?.title ?? `submission ${submission.id}`;
    this.reviewingKey.set(group.key);
    this.eventsAdmin
      .reviewSubmission(submission.id, action, decision)
      .pipe(takeUntilDestroyed(this.destroyRef))
      .subscribe({
        next: (result) => {
          this.reviewingKey.set(null);
          this.removeFromQueue(result.decided_event_ids);
          const count = result.decided_event_ids.length;
          const dates = count === 1 ? '1 date' : `all ${count} dates`;
          this.toast.success(
            `${action === 'approve' ? 'Approved' : 'Rejected'} ${dates} of "${title}".` +
              emailedNote(result.decision_emailed),
          );
        },
        error: () => {
          this.reviewingKey.set(null);
          this.toast.error(`Couldn't ${action} "${title}".`);
        },
      });
  }

  private removeFromQueue(eventIds: number[]): void {
    const decided = new Set(eventIds);
    this.pendingRows.update((rows) => rows.filter((row) => !decided.has(Number(row['id']))));
  }

  private loadQueue(): void {
    this.loadingQueue.set(true);
    this.crud
      .getFilteredTableRows('events', 'starts_at_us', true, PAGE_SIZE, 0, [
        { column_name: 'status', column_value: 'pending' },
      ])
      .pipe(
        map((result) => dataResultsToRecords(result)),
        switchMap((rows) => this.loadSubmissionDetails(rows).pipe(map(() => rows))),
        takeUntilDestroyed(this.destroyRef),
      )
      .subscribe({
        next: (rows) => {
          this.pendingRows.set(rows);
          this.loadingQueue.set(false);
        },
        error: () => {
          this.pendingRows.set([]);
          this.loadingQueue.set(false);
          this.toast.error("The pending queue couldn't be loaded.");
        },
      });
  }

  // For the submitted events in the queue: their submissions, their
  // submitters' names, and the latest approved events (the duplicate flag).
  // Any of these that can't be read leaves its part of the queue plainer, not
  // broken.
  private loadSubmissionDetails(rows: Record<string, string>[]): Observable<unknown> {
    const submissionIds = [
      ...new Set(rows.map((row) => row['submission_id']).filter((id) => !!id)),
    ];
    if (submissionIds.length === 0) {
      return of(null);
    }
    const submissions$ = forkJoin(
      submissionIds.map((id) =>
        this.crud.getRow('event_submissions', 'id', id).pipe(
          map((result) => dataResultsToRecords(result)[0]),
          catchError(() => of(undefined)),
        ),
      ),
    );
    const approved$ = this.crud
      .getFilteredTableRows('events', 'starts_at_us', false, PAGE_SIZE, 0, [
        { column_name: 'status', column_value: 'approved' },
      ])
      .pipe(
        map((result) => dataResultsToRecords(result)),
        catchError(() => of<Record<string, string>[]>([])),
      );
    return forkJoin({ submissions: submissions$, approved: approved$ }).pipe(
      switchMap(({ submissions, approved }) => {
        const byId: Record<string, Record<string, string>> = {};
        for (const submission of submissions) {
          if (submission) {
            byId[submission['id']] = submission;
          }
        }
        this.submissionRows.set(byId);
        this.approvedRows.set(approved);
        const personIds = [
          ...new Set(Object.values(byId).map((submission) => submission['submitted_by_person_id'])),
        ].filter((id) => !!id);
        return personIds.length
          ? this.crud.resolveFkDisplay('people', personIds).pipe(
              map((response) => this.submitterNames.set(response.resolved)),
              catchError(() => of(null)),
            )
          : of(null);
      }),
    );
  }

  private toPendingEvent(
    row: Record<string, string>,
    candidates: DuplicateCandidate[],
  ): PendingEvent {
    const startsAtUs = Number(row['starts_at_us']);
    const endsAtUs = row['ends_at_us'] ? Number(row['ends_at_us']) : null;
    const venue = this.venues().find((option) => option.value === row['venue_id']);
    const id = Number(row['id']);
    const submissionId = row['submission_id'] ? Number(row['submission_id']) : null;
    const event: PendingEvent = {
      id,
      title: row['title'] ?? '',
      when: startsAtUs
        ? `${formatEventShortDate(startsAtUs)} · ${formatEventTimeRange(startsAtUs, endsAtUs)}`
        : '',
      where: venue?.display ?? row['location_text'] ?? '',
      description: row['description'] ?? '',
      url: row['url'] ?? '',
      costText: row['cost_text'] ?? '',
      submissionId,
      possibleDuplicates: [],
      noLink: false,
    };
    if (submissionId === null) {
      return event;
    }
    // Another event in the queue or on the calendar, but not one of this
    // submission's own dates.
    const self = toCandidate(row);
    const ownDates = new Set(
      this.pendingRows()
        .filter((other) => other['submission_id'] === row['submission_id'])
        .map((other) => Number(other['id'])),
    );
    event.possibleDuplicates = candidates
      .filter((other) => !ownDates.has(other.id) && isPossibleDuplicate(self, other))
      .map((other) => ({ id: other.id, title: other.title }));
    event.noLink = !(row['url'] ?? '').trim();
    return event;
  }

  // The queue in start order, a submission's dates together where its first
  // date falls.
  private groupQueue(events: PendingEvent[]): QueueGroup[] {
    const groups: UnflaggedGroup[] = [];
    const bySubmission = new Map<number, UnflaggedGroup>();
    for (const event of events) {
      if (event.submissionId === null) {
        groups.push({ key: `e${event.id}`, submission: null, events: [event] });
        continue;
      }
      let group = bySubmission.get(event.submissionId);
      if (!group) {
        group = {
          key: `s${event.submissionId}`,
          submission: this.toSubmissionSummary(event.submissionId),
          events: [],
        };
        bySubmission.set(event.submissionId, group);
        groups.push(group);
      }
      group.events.push(event);
    }
    return groups.map(withFlags);
  }

  private toSubmissionSummary(submissionId: number): SubmissionSummary {
    const row = this.submissionRows()[String(submissionId)] ?? {};
    const personId = row['submitted_by_person_id'] ?? '';
    return {
      id: submissionId,
      submitter: this.submitterNames()[personId] || (personId ? `member #${personId}` : 'a member'),
      relationship: relationshipLabel(row['relationship'] ?? ''),
      note: row['submitter_note'] ?? '',
      regularThing: row['regular_thing'] === 't' || row['regular_thing'] === 'true',
    };
  }

  private loadPickers(): void {
    forkJoin({
      venues: this.crud.getFkOptions('venues', '', PAGE_SIZE),
      series: this.crud.getFkOptions('series', '', PAGE_SIZE),
    })
      .pipe(takeUntilDestroyed(this.destroyRef))
      .subscribe({
        next: ({ venues, series }) => {
          this.venues.set(venues.options);
          this.series.set(series.options);
        },
        error: () => this.toast.error("The venue and series lists couldn't be loaded."),
      });
  }
}

type UnflaggedGroup = Omit<QueueGroup, 'possibleDuplicates' | 'noLink'>;

// A card's flags: every possible duplicate of any of its dates (each once),
// and "No link" if any date has none.
function withFlags(group: UnflaggedGroup): QueueGroup {
  const duplicates = new Map<number, DuplicateOf>();
  for (const event of group.events) {
    for (const duplicate of event.possibleDuplicates) {
      duplicates.set(duplicate.id, duplicate);
    }
  }
  return {
    ...group,
    possibleDuplicates: [...duplicates.values()],
    noLink: group.events.some((event) => event.noLink),
  };
}

function toCandidate(row: Record<string, string>): DuplicateCandidate {
  return {
    id: Number(row['id']),
    title: row['title'] ?? '',
    startsAtUs: Number(row['starts_at_us']),
    venueId: row['venue_id'] ? Number(row['venue_id']) : null,
  };
}

function targetKey(target: RejectTarget): string {
  return target.kind === 'event' ? `e${target.event.id}` : target.group.key;
}

// The toast's word on the decision email (`decision_emailed`).
function emailedNote(emailed: boolean): string {
  return emailed ? ' The submitter has been emailed.' : '';
}
