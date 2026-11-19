import { Component, computed, DestroyRef, inject, OnInit, signal, viewChild } from '@angular/core';
import { takeUntilDestroyed } from '@angular/core/rxjs-interop';
import { FormArray, FormControl, FormGroup, ReactiveFormsModule } from '@angular/forms';
import { RouterLink } from '@angular/router';
import { catchError, defer, map, Observable, of, switchMap } from 'rxjs';
import {
  MatAutocompleteModule,
  MatAutocompleteSelectedEvent,
} from '@angular/material/autocomplete';
import { MatButtonModule } from '@angular/material/button';
import { MatCheckboxModule } from '@angular/material/checkbox';
import { MatChipsModule } from '@angular/material/chips';
import { provideNativeDateAdapter } from '@angular/material/core';
import { MAT_FORM_FIELD_DEFAULT_OPTIONS, MatFormFieldModule } from '@angular/material/form-field';
import { MatIconModule } from '@angular/material/icon';
import { MatInputModule } from '@angular/material/input';
import { MatProgressSpinnerModule } from '@angular/material/progress-spinner';
import { MatRadioModule } from '@angular/material/radio';
import { MatSelectModule } from '@angular/material/select';
import { AuthService } from '@honuware/ui/auth';
import { SimpleDateComponent } from '@honuware/ui/controls';
import { PhotoUploadComponent } from '@honuware/ui/photos';
import { COMMUNITY_ACCESS } from '../../core/community-access/community-access';
import {
  EMAIL_NOT_VERIFIED,
  EVENT_SUBMISSIONS_ACCESS,
  FlyerImageType,
  SUBMISSIONS_PAUSED,
  SubmissionProblem,
  SubmissionRelationship,
  SubmissionResult,
  toSubmissionProblem,
} from '../../core/event-submissions-access/event-submissions-access';
import {
  EMPTY_VOCABULARIES,
  EventVocabularies,
  EventVocabulariesService,
} from '../../core/services/event-vocabularies.service';
import { VenueChoice } from '../../core/types/events.types';
import {
  datePickerValue,
  pickedDateText,
  pickedTimeText,
  pickerField,
  timePickerValue,
} from '../../core/utils/date-picker-values';
import { eventPagePath } from '../../core/utils/event-links';
import { formatEventShortDate, formatEventTime } from '../../core/utils/events-date.utils';
import { PageMeta } from '../../core/utils/page-meta';
import { RELATIONSHIPS } from '../../core/utils/submission-labels';
import { VideoFieldFeedback, videoFieldFeedback } from '../../core/utils/video-links';
import { nextDayEndNote } from '../admin/manage-events-form';
import {
  composeSubmission,
  dateRowTimes,
  EMPTY_SUBMIT_DATE,
  FLYER_TYPE_MESSAGE,
  flyerImageType,
  MAX_SUBMISSION_DATES,
  PlaceMode,
  SubmitDateValue,
  SubmitFormValue,
} from './submit-event-form';

// How the flyer went, said on the thanks panel; null when none was chosen.
export interface FlyerOutcome {
  ok: boolean;
  message: string;
}

// At most this many categories, and this many scenes (the server's rule).
export const MAX_TAGS_PER_KIND = 3;

// The form's fields the server names in a refusal, by their JSON names. A
// refusal about anything else (a rate limit) shows beside the Send button.
const FORM_FIELDS = [
  'title',
  'dates',
  'venue_id',
  'location_text',
  'description',
  'url',
  'cost_text',
  'video_url',
  // Never named by the server (a flyer goes up after the send); the browser's
  // own type check.
  'flyer',
  'category_slugs',
  'scene_tag_slugs',
  'relationship',
  'submitter_note',
  'agreed',
];

type PageState = 'loading' | 'open' | 'paused';

// A row's value when it was made is its controls' defaultValue, which is what
// its pickers start from (see pickerStart).
function dateGroup(value: SubmitDateValue = EMPTY_SUBMIT_DATE) {
  return new FormGroup({
    date: new FormControl(value.date, { nonNullable: true }),
    startTime: new FormControl(value.startTime, { nonNullable: true }),
    endDate: new FormControl(value.endDate, { nonNullable: true }),
    endTime: new FormControl(value.endTime, { nonNullable: true }),
  });
}

// /events/submit (EV3 §5.2): the one way an event reaches the calendar from
// outside. Signed-in members with a confirmed email send a set of details on
// 1–12 dates; each date waits in the curators' queue.
//
// - It checks on load whether submissions are open. While curators have
//   paused them it shows the notice instead of the form, signed in or not. A
//   pause that lands while the form is open (a 503 on send) shows the same
//   notice and keeps what was typed.
// - Signed out, it explains and offers sign-in (which comes back here) and
//   registration.
// - A refusal shows next to the field it names, and a date's next to its row.
// - After sending: thanks, any listed events that look the same, and links to
//   "My submissions" and another submission.
// - CM3 §5.7: an optional video link, checked as it's typed and sent in its
//   canonical form, and an optional flyer (a JPEG or PNG, picked with
//   hw-photo-upload held back). The flyer goes up only once the submission is
//   in, to /api/event_submissions/<id>/photo/<type>, which puts it on every
//   pending date; if that fails the submission still stands and the thanks
//   says the flyer didn't make it.
@Component({
  selector: 'app-submit-event',
  imports: [
    RouterLink,
    ReactiveFormsModule,
    MatAutocompleteModule,
    MatButtonModule,
    MatCheckboxModule,
    MatChipsModule,
    MatFormFieldModule,
    MatIconModule,
    MatInputModule,
    MatProgressSpinnerModule,
    MatRadioModule,
    MatSelectModule,
    PhotoUploadComponent,
    SimpleDateComponent,
  ],
  providers: [
    // The honuware pickers' own date-adapter import doesn't reach them inside
    // another component's template, so the page provides one.
    provideNativeDateAdapter(),
    // They draw their own form fields; this gives them the page's outline look.
    { provide: MAT_FORM_FIELD_DEFAULT_OPTIONS, useValue: { appearance: 'outline' } },
  ],
  templateUrl: './submit-event.html',
  styleUrl: './submit-event.scss',
})
export class SubmitEventPage implements OnInit {
  private submissions = inject(EVENT_SUBMISSIONS_ACCESS);
  private community = inject(COMMUNITY_ACCESS);
  private vocabularyService = inject(EventVocabulariesService);
  private auth = inject(AuthService);
  private pageMeta = inject(PageMeta);
  private destroyRef = inject(DestroyRef);

  readonly maxTags = MAX_TAGS_PER_KIND;

  // Getters, not fields, for the imported constants: the unit-test runner's
  // module transform (Vite SSR) leaves an import used as a bare class-field
  // initializer unrewritten, so it reads undefined when the import lands in
  // another chunk. The app build is unaffected.
  get maxDates(): number {
    return MAX_SUBMISSION_DATES;
  }

  get relationships(): typeof RELATIONSHIPS {
    return RELATIONSHIPS;
  }

  readonly startPickerField = pickerField('Start', true);
  readonly endPickerField = pickerField('End', false);

  readonly state = signal<PageState>('loading');
  readonly signedIn = signal(false);
  readonly unverified = signal(false);
  readonly sending = signal(false);
  readonly sent = signal<SubmissionResult | null>(null);
  readonly flyerOutcome = signal<FlyerOutcome | null>(null);

  private readonly flyerPicker = viewChild(PhotoUploadComponent);

  // Refusals: by field (JSON name), by date row, and for the form as a whole.
  readonly fieldErrors = signal<Record<string, string>>({});
  readonly dateErrors = signal<Record<number, string>>({});
  readonly formError = signal('');

  readonly vocabularies = signal<EventVocabularies>(EMPTY_VOCABULARIES);
  readonly venues = signal<VenueChoice[]>([]);
  private readonly venueSearch = signal('');
  readonly venueOptions = computed(() => {
    const search = this.venueSearch().trim().toLowerCase();
    return this.venues().filter(
      (venue) =>
        !search ||
        venue.name.toLowerCase().includes(search) ||
        venue.address.toLowerCase().includes(search),
    );
  });
  readonly venueId = signal<number | null>(null);
  readonly categorySlugs = signal<string[]>([]);
  readonly sceneTagSlugs = signal<string[]>([]);

  readonly form = new FormGroup({
    title: new FormControl('', { nonNullable: true }),
    dates: new FormArray([dateGroup()]),
    placeMode: new FormControl<PlaceMode>('venue', { nonNullable: true }),
    venue: new FormControl<string | VenueChoice>('', { nonNullable: true }),
    locationText: new FormControl('', { nonNullable: true }),
    description: new FormControl('', { nonNullable: true }),
    url: new FormControl('', { nonNullable: true }),
    costText: new FormControl('', { nonNullable: true }),
    videoUrl: new FormControl('', { nonNullable: true }),
    regularThing: new FormControl(false, { nonNullable: true }),
    relationship: new FormControl<SubmissionRelationship>('organizer', { nonNullable: true }),
    submitterNote: new FormControl('', { nonNullable: true }),
    agreed: new FormControl(false, { nonNullable: true }),
    website: new FormControl('', { nonNullable: true }),
  });

  get dateRows() {
    return this.form.controls.dates;
  }

  ngOnInit(): void {
    this.pageMeta.set({
      title: 'Submit an event',
      description:
        'Tell us about an event for the community calendar. Curators review every ' +
        'submission before it is listed.',
      canonicalPath: '/events/submit',
    });
    this.auth.authData$
      .pipe(takeUntilDestroyed(this.destroyRef))
      .subscribe((data) => this.signedIn.set(data.isAuth));
    this.checkStatus();
    this.vocabularyService.vocabularies$
      .pipe(takeUntilDestroyed(this.destroyRef))
      .subscribe((vocabularies) => this.vocabularies.set(vocabularies));
    this.community
      .getVenueChoices()
      .pipe(takeUntilDestroyed(this.destroyRef))
      .subscribe({
        next: (venues) => this.venues.set(venues),
        error: () => this.venues.set([]),
      });
    // Typing in the venue box searches; editing a picked venue unpicks it.
    this.form.controls.venue.valueChanges
      .pipe(takeUntilDestroyed(this.destroyRef))
      .subscribe((value) => {
        if (typeof value === 'string') {
          this.venueSearch.set(value);
          this.venueId.set(null);
        }
      });
  }

  // On load, and from the paused notice's "Check again". A status that
  // can't be read counts as open: the send will say if it isn't.
  checkStatus(): void {
    this.submissions
      .getStatus()
      .pipe(takeUntilDestroyed(this.destroyRef))
      .subscribe({
        next: (accepting) => (accepting ? this.showForm() : this.state.set('paused')),
        error: () => this.showForm(),
      });
  }

  // A picker's value → its row's control, on a pick and as it's typed in.
  // The picker announces only readable values, so typing (which can clear the
  // box) reads the picker's own value instead.
  syncPicked(control: FormControl<string>, picker: SimpleDateComponent): void {
    const value = picker.dateControl.value;
    control.setValue(picker.type === 'time' ? pickedTimeText(value) : pickedDateText(value));
  }

  // What a picker starts from: its row's value when the row was made. Not the
  // live value, since feeding each pick back in would rewrite the box while
  // it's being typed in.
  pickerStart(control: FormControl<string>, type: 'date' | 'time'): string {
    return type === 'time'
      ? timePickerValue(control.defaultValue)
      : datePickerValue(control.defaultValue);
  }

  displayVenue(venue: VenueChoice | string): string {
    return typeof venue === 'string' ? venue : (venue?.name ?? '');
  }

  onVenueSelected(event: MatAutocompleteSelectedEvent): void {
    this.venueId.set((event.option.value as VenueChoice).id);
  }

  addDate(): void {
    if (this.dateRows.length < MAX_SUBMISSION_DATES) {
      this.dateRows.push(dateGroup());
    }
  }

  removeDate(index: number): void {
    if (this.dateRows.length > 1) {
      this.dateRows.removeAt(index);
      this.dateErrors.set({});
    }
  }

  isCategorySelected(slug: string): boolean {
    return this.categorySlugs().includes(slug);
  }

  isSceneTagSelected(slug: string): boolean {
    return this.sceneTagSlugs().includes(slug);
  }

  toggleCategory(slug: string, selected: boolean): void {
    this.categorySlugs.update((slugs) => toggled(slugs, slug, selected));
  }

  toggleSceneTag(slug: string, selected: boolean): void {
    this.sceneTagSlugs.update((slugs) => toggled(slugs, slug, selected));
  }

  fieldError(field: string): string {
    return this.fieldErrors()[field] ?? '';
  }

  dateError(index: number): string {
    return this.dateErrors()[index] ?? '';
  }

  // "Ends the next day: …" under a row whose end time rolls past midnight.
  endNote(row: SubmitDateValue): string {
    return nextDayEndNote(dateRowTimes(row));
  }

  // Under the video field, as it's typed: '' while blank.
  videoFeedback(): VideoFieldFeedback {
    return videoFieldFeedback(this.form.controls.videoUrl.value);
  }

  send(): void {
    this.clearErrors();
    const composed = composeSubmission(this.formValue());
    if (!composed.ok) {
      this.showProblem(composed.field, composed.dateIndex, composed.message);
      return;
    }
    // Taken now: the picker goes away with the form once it's sent.
    const flyer = this.flyerPicker()?.pendingFile ?? null;
    const flyerType = flyer ? flyerImageType(flyer) : null;
    if (flyer && !flyerType) {
      this.showProblem('flyer', null, FLYER_TYPE_MESSAGE);
      return;
    }
    this.sending.set(true);
    this.submissions
      .submit(composed.request)
      .pipe(
        switchMap((result) =>
          this.attachFlyer(result, flyer, flyerType).pipe(
            map((flyerOutcome) => ({ result, flyerOutcome })),
          ),
        ),
        takeUntilDestroyed(this.destroyRef),
      )
      .subscribe({
        next: ({ result, flyerOutcome }) => {
          this.sending.set(false);
          this.flyerOutcome.set(flyerOutcome);
          this.sent.set(result);
        },
        error: (error: unknown) => {
          this.sending.set(false);
          this.onRefused(toSubmissionProblem(error));
        },
      });
  }

  // "Submit another": a fresh form.
  startOver(): void {
    this.sent.set(null);
    this.flyerOutcome.set(null);
    // The thanks panel normally takes the picker away; clear it all the same.
    this.flyerPicker()?.clearPendingFile();
    this.clearErrors();
    this.form.reset();
    this.dateRows.clear();
    this.dateRows.push(dateGroup());
    this.venueId.set(null);
    this.categorySlugs.set([]);
    this.sceneTagSlugs.set([]);
  }

  eventPath(eventId: number): string {
    return eventPagePath(eventId);
  }

  when(startsAtUs: number): string {
    return `${formatEventShortDate(startsAtUs)}, ${formatEventTime(startsAtUs)}`;
  }

  // Which submitted date a duplicate hint is about, as the form numbered them.
  dateLabel(dateIndex: number): string {
    const result = this.sent();
    return result && result.event_ids.length > 1 ? ` (your date ${dateIndex + 1})` : '';
  }

  ignoredTags(): boolean {
    const result = this.sent();
    return (
      !!result && result.ignored_category_slugs.length + result.ignored_scene_tag_slugs.length > 0
    );
  }

  // The form, on load or back after a pause. The pickers take a value only
  // when they're made, so the date rows are rebuilt to start from what was
  // typed before the form went away.
  private showForm(): void {
    const rows = this.dateRows.getRawValue();
    this.dateRows.clear();
    for (const row of rows) {
      this.dateRows.push(dateGroup(row));
    }
    this.state.set('open');
  }

  // The flyer, once the submission is in. Never an error: the submission
  // stands either way, and the outcome says how the flyer went. A honeypot
  // send (submission_id 0) has nothing to attach to, and is told nothing.
  private attachFlyer(
    result: SubmissionResult,
    flyer: File | null,
    flyerType: FlyerImageType | null,
  ): Observable<FlyerOutcome | null> {
    if (!flyer || !flyerType || !result.submission_id) {
      return of(null);
    }
    return defer(() => flyer.arrayBuffer()).pipe(
      switchMap((bytes) => this.submissions.attachFlyer(result.submission_id, bytes, flyerType)),
      map(() => ({ ok: true, message: 'Your flyer is attached.' })),
      catchError((error: unknown) =>
        of({
          ok: false,
          message:
            "Your event was sent, but its flyer couldn't be added. " +
            (toSubmissionProblem(error).detail || 'Please try again later.'),
        }),
      ),
    );
  }

  private onRefused(problem: SubmissionProblem): void {
    if (problem.error_code === SUBMISSIONS_PAUSED) {
      // The form's values stay in `form`; "Check again" brings it back.
      this.state.set('paused');
      return;
    }
    if (problem.error_code === EMAIL_NOT_VERIFIED) {
      this.unverified.set(true);
      return;
    }
    this.showProblem(problem.field, problem.date_index, problem.detail);
  }

  private showProblem(field: string, dateIndex: number | null, message: string): void {
    if (field === 'dates' && dateIndex !== null) {
      this.dateErrors.set({ [dateIndex]: message });
    } else if (FORM_FIELDS.includes(field)) {
      this.fieldErrors.set({ [field]: message });
    } else {
      this.formError.set(message);
    }
  }

  private clearErrors(): void {
    this.fieldErrors.set({});
    this.dateErrors.set({});
    this.formError.set('');
  }

  private formValue(): SubmitFormValue {
    const raw = this.form.getRawValue();
    return {
      title: raw.title,
      dates: raw.dates,
      placeMode: raw.placeMode,
      venueId: this.venueId(),
      locationText: raw.locationText,
      description: raw.description,
      url: raw.url,
      costText: raw.costText,
      videoUrl: raw.videoUrl,
      categorySlugs: this.categorySlugs(),
      sceneTagSlugs: this.sceneTagSlugs(),
      regularThing: raw.regularThing,
      relationship: raw.relationship,
      submitterNote: raw.submitterNote,
      agreed: raw.agreed,
      website: raw.website,
    };
  }
}

function toggled(slugs: string[], slug: string, selected: boolean): string[] {
  const without = slugs.filter((existing) => existing !== slug);
  return selected ? [...without, slug] : without;
}
