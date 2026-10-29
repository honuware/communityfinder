import { Component, computed, DestroyRef, inject, OnInit, signal } from '@angular/core';
import { takeUntilDestroyed } from '@angular/core/rxjs-interop';
import { FormControl, FormGroup, ReactiveFormsModule, Validators } from '@angular/forms';
import { ActivatedRoute, RouterLink } from '@angular/router';
import {
  concat,
  debounceTime,
  distinctUntilChanged,
  filter,
  forkJoin,
  last,
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
import { MatCardModule } from '@angular/material/card';
import { MatCheckboxModule } from '@angular/material/checkbox';
import { MatFormFieldModule } from '@angular/material/form-field';
import { MatIconModule } from '@angular/material/icon';
import { MatInputModule } from '@angular/material/input';
import { MatProgressSpinnerModule } from '@angular/material/progress-spinner';
import { MatSelectModule } from '@angular/material/select';
import {
  DataResults,
  FkOption,
  FkOptionsResponse,
  HONUWARE_CRUD_ACCESS,
} from '@honuware/ui/access';
import { AuthService } from '@honuware/ui/auth';
import { ToastService } from '@honuware/ui/foundation';
import {
  EVENTS_ADMIN_ACCESS,
  ReviewAction,
} from '../../core/events-admin-access/events-admin-access';
import { EventVocabulariesService } from '../../core/services/event-vocabularies.service';
import { dataResultsToRecords } from '../../core/types/events.types';
import { eventPagePath } from '../../core/utils/event-links';
import { formatEventShortDate, formatEventTime } from '../../core/utils/events-date.utils';
import {
  AssignmentRow,
  assignmentChanges,
  composeEventTimes,
  EMPTY_EVENT_FORM,
  eventFormFromRow,
  EventFormValue,
  eventRowValue,
  rowChanges,
  rowForCreate,
} from './manage-events-form';

// One junction table: an event's categories, or its scene tags.
interface AssignmentTable {
  table: string;
  tagColumn: string;
}

const CATEGORY_ASSIGNMENTS: AssignmentTable = {
  table: 'event_category_assignments',
  tagColumn: 'category_id',
};
const SCENE_TAG_ASSIGNMENTS: AssignmentTable = {
  table: 'event_scene_tag_assignments',
  tagColumn: 'scene_tag_id',
};

// Enough rows for any one event's tags, or the whole pending queue.
const PAGE_SIZE = 500;

interface PendingEvent {
  id: number;
  title: string;
  // "Fri, Aug 14 · 10:00 PM · The Lantern Room"
  summary: string;
}

// The bespoke event editor (/admin/events, EV1 OQ2): the generic CRUD
// editor's date control can't express a time of day, so events get their
// own page. Like ManageRolesPage and ManageUsersPage, it talks to the generic
// HONUWARE_CRUD_ACCESS directly, plus the one bespoke call, the review
// endpoint. No new server surface.
//
// - The pending queue: approve, or reject with a note.
// - The form: new or existing (find one by title, open one from the queue,
//   or arrive from an event page's "Edit" link as ?edit=<id>). Times are
//   entered as Seattle wall-clock time and stored as `_us` instants.
// - Category and scene checkboxes write the two assignment tables. Left all
//   unchecked, an event in a series shows the series' own tags (EV1's
//   inheritance rule).
//
// A new event is saved as pending, like any other; it goes public once
// approved in the queue.
@Component({
  selector: 'app-manage-events',
  imports: [
    RouterLink,
    ReactiveFormsModule,
    MatAutocompleteModule,
    MatButtonModule,
    MatCardModule,
    MatCheckboxModule,
    MatFormFieldModule,
    MatIconModule,
    MatInputModule,
    MatProgressSpinnerModule,
    MatSelectModule,
  ],
  templateUrl: './manage-events.html',
  styleUrl: './manage-events.scss',
})
export class ManageEventsPage implements OnInit {
  private crud = inject(HONUWARE_CRUD_ACCESS);
  private eventsAdmin = inject(EVENTS_ADMIN_ACCESS);
  private vocabularyService = inject(EventVocabulariesService);
  private auth = inject(AuthService);
  private toast = inject(ToastService);
  private route = inject(ActivatedRoute);
  private destroyRef = inject(DestroyRef);

  // The pending events' rows, as the generic table read sends them.
  private readonly pendingRows = signal<Record<string, string>[]>([]);
  readonly loadingQueue = signal(true);
  readonly reviewingId = signal<number | null>(null);

  readonly venues = signal<FkOption[]>([]);
  readonly pending = computed(() => this.pendingRows().map((row) => this.toPendingEvent(row)));
  readonly series = signal<FkOption[]>([]);
  readonly categories = signal<{ id: number; label: string }[]>([]);
  readonly sceneTags = signal<{ id: number; label: string }[]>([]);
  readonly selectedCategoryIds = signal<number[]>([]);
  readonly selectedSceneTagIds = signal<number[]>([]);

  readonly searchControl = new FormControl<string | FkOption>('', { nonNullable: true });
  readonly searchOptions = signal<FkOption[]>([]);

  // The event being edited; null for a new one.
  readonly editingId = signal<string | null>(null);
  readonly saving = signal(false);
  readonly formError = signal('');

  readonly form = new FormGroup({
    title: new FormControl('', { nonNullable: true, validators: [Validators.required] }),
    description: new FormControl('', { nonNullable: true }),
    startDate: new FormControl('', { nonNullable: true, validators: [Validators.required] }),
    startTime: new FormControl('', { nonNullable: true, validators: [Validators.required] }),
    endDate: new FormControl('', { nonNullable: true }),
    endTime: new FormControl('', { nonNullable: true }),
    venueId: new FormControl('', { nonNullable: true }),
    seriesId: new FormControl('', { nonNullable: true }),
    locationText: new FormControl('', { nonNullable: true }),
    url: new FormControl('', { nonNullable: true }),
    costText: new FormControl('', { nonNullable: true }),
  });

  // The row and assignments as loaded, to save only what changed.
  private original: Record<string, string> = {};
  private originalCategories: AssignmentRow[] = [];
  private originalSceneTags: AssignmentRow[] = [];

  ngOnInit(): void {
    this.loadQueue();
    this.loadPickers();
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
        this.openEvent(id);
      }
    });
  }

  displayOption(option: FkOption | string): string {
    return typeof option === 'string' ? option : (option?.display ?? '');
  }

  onSearchSelected(event: MatAutocompleteSelectedEvent): void {
    this.openEvent((event.option.value as FkOption).value);
  }

  newEvent(): void {
    this.editingId.set(null);
    this.original = {};
    this.originalCategories = [];
    this.originalSceneTags = [];
    this.form.reset(EMPTY_EVENT_FORM);
    this.selectedCategoryIds.set([]);
    this.selectedSceneTagIds.set([]);
    this.formError.set('');
  }

  openEvent(id: string): void {
    this.formError.set('');
    forkJoin({
      row: this.crud.getRow('events', 'id', id),
      categories: this.loadAssignments(CATEGORY_ASSIGNMENTS, id),
      sceneTags: this.loadAssignments(SCENE_TAG_ASSIGNMENTS, id),
    })
      .pipe(takeUntilDestroyed(this.destroyRef))
      .subscribe({
        next: ({ row, categories, sceneTags }) => {
          const record = dataResultsToRecords(row)[0];
          if (!record) {
            this.formError.set(`Event ${id} wasn't found.`);
            return;
          }
          this.editingId.set(id);
          this.original = record;
          this.originalCategories = categories;
          this.originalSceneTags = sceneTags;
          this.form.reset(eventFormFromRow(record));
          this.selectedCategoryIds.set(categories.map((row) => row.tagId));
          this.selectedSceneTagIds.set(sceneTags.map((row) => row.tagId));
          this.searchControl.setValue('', { emitEvent: false });
        },
        error: () => this.formError.set(`Event ${id} couldn't be loaded.`),
      });
  }

  isCategorySelected(id: number): boolean {
    return this.selectedCategoryIds().includes(id);
  }

  isSceneTagSelected(id: number): boolean {
    return this.selectedSceneTagIds().includes(id);
  }

  toggleCategory(id: number, checked: boolean): void {
    this.selectedCategoryIds.update((ids) => toggled(ids, id, checked));
  }

  toggleSceneTag(id: number, checked: boolean): void {
    this.selectedSceneTagIds.update((ids) => toggled(ids, id, checked));
  }

  approve(item: PendingEvent): void {
    this.review(item, 'approve', '');
  }

  reject(item: PendingEvent): void {
    const notes = window.prompt(
      `Reject "${item.title}"? Add a note for the record (optional):`,
      '',
    );
    if (notes !== null) {
      this.review(item, 'reject', notes);
    }
  }

  previewPath(id: number | string): string {
    return eventPagePath(Number(id));
  }

  save(): void {
    if (this.form.invalid) {
      this.form.markAllAsTouched();
      this.formError.set('Fill in the title and the start date and time.');
      return;
    }
    const value: EventFormValue = this.form.getRawValue();
    const times = composeEventTimes(value);
    if (!times.ok) {
      this.formError.set(times.error);
      return;
    }
    const row = eventRowValue(value, times);
    const editingId = this.editingId();
    let saveRow$: Observable<string>;
    if (editingId) {
      const changes = rowChanges(row, this.original);
      saveRow$ = Object.keys(changes).length
        ? this.crud
            .updateItem({
              table_name: 'events',
              column_name: 'id',
              column_value: editingId,
              value: changes,
            })
            .pipe(map(() => editingId))
        : of(editingId);
    } else {
      const authData = this.auth.authData;
      saveRow$ = this.crud.addItemFetchPrimaryKey({
        table_name: 'events',
        value: rowForCreate(row, authData.isAuth ? authData.personId : null),
      });
    }

    this.saving.set(true);
    this.formError.set('');
    saveRow$
      .pipe(
        switchMap((eventId) => this.saveAssignments(eventId).pipe(map(() => eventId))),
        takeUntilDestroyed(this.destroyRef),
      )
      .subscribe({
        next: (eventId) => {
          this.saving.set(false);
          this.toast.success(
            editingId ? 'Event saved.' : 'Event added. It goes public once approved in the queue.',
          );
          this.loadQueue();
          this.openEvent(eventId);
        },
        error: () => {
          this.saving.set(false);
          this.formError.set("The event couldn't be saved. Check the fields and try again.");
        },
      });
  }

  private review(item: PendingEvent, action: ReviewAction, notes: string): void {
    this.reviewingId.set(item.id);
    this.eventsAdmin
      .reviewEvent(item.id, action, notes)
      .pipe(takeUntilDestroyed(this.destroyRef))
      .subscribe({
        next: () => {
          this.reviewingId.set(null);
          this.pendingRows.update((rows) => rows.filter((row) => Number(row['id']) !== item.id));
          this.toast.success(
            action === 'approve' ? `Approved "${item.title}".` : `Rejected "${item.title}".`,
          );
        },
        error: () => {
          this.reviewingId.set(null);
          this.toast.error(`Couldn't ${action} "${item.title}".`);
        },
      });
  }

  private loadQueue(): void {
    this.loadingQueue.set(true);
    this.crud
      .getFilteredTableRows('events', 'starts_at_us', true, PAGE_SIZE, 0, [
        { column_name: 'status', column_value: 'pending' },
      ])
      .pipe(takeUntilDestroyed(this.destroyRef))
      .subscribe({
        next: (result) => {
          this.pendingRows.set(dataResultsToRecords(result));
          this.loadingQueue.set(false);
        },
        error: () => {
          this.pendingRows.set([]);
          this.loadingQueue.set(false);
          this.toast.error("The pending queue couldn't be loaded.");
        },
      });
  }

  private toPendingEvent(row: Record<string, string>): PendingEvent {
    const startsAtUs = Number(row['starts_at_us']);
    const venue = this.venues().find((option) => option.value === row['venue_id']);
    return {
      id: Number(row['id']),
      title: row['title'] ?? '',
      summary: [
        startsAtUs ? formatEventShortDate(startsAtUs) : '',
        startsAtUs ? formatEventTime(startsAtUs) : '',
        venue?.display ?? row['location_text'] ?? '',
      ]
        .filter(Boolean)
        .join(' · '),
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

  private loadAssignments(table: AssignmentTable, eventId: string): Observable<AssignmentRow[]> {
    return this.crud
      .getFilteredTableRows(table.table, 'id', true, PAGE_SIZE, 0, [
        { column_name: 'event_id', column_value: eventId },
      ])
      .pipe(
        map((result: DataResults) =>
          dataResultsToRecords(result).map((row) => ({
            id: row['id'],
            tagId: Number(row[table.tagColumn]),
          })),
        ),
      );
  }

  // The checkbox changes as assignment rows added and deleted, one at a time.
  private saveAssignments(eventId: string): Observable<unknown> {
    const writes = [
      ...this.assignmentWrites(
        CATEGORY_ASSIGNMENTS,
        eventId,
        this.editingId() ? this.originalCategories : [],
        this.categories().map((tag) => tag.id),
        this.selectedCategoryIds(),
      ),
      ...this.assignmentWrites(
        SCENE_TAG_ASSIGNMENTS,
        eventId,
        this.editingId() ? this.originalSceneTags : [],
        this.sceneTags().map((tag) => tag.id),
        this.selectedSceneTagIds(),
      ),
    ];
    return writes.length ? concat(...writes).pipe(last()) : of(null);
  }

  private assignmentWrites(
    table: AssignmentTable,
    eventId: string,
    existing: AssignmentRow[],
    shownTagIds: number[],
    selectedTagIds: number[],
  ): Observable<void>[] {
    const changes = assignmentChanges(existing, shownTagIds, selectedTagIds);
    return [
      ...changes.remove.map((rowId) => this.crud.deleteItem(table.table, 'id', rowId)),
      ...changes.add.map((tagId) =>
        this.crud.addItem({
          table_name: table.table,
          value: { event_id: eventId, [table.tagColumn]: String(tagId) },
        }),
      ),
    ];
  }
}

function toggled(ids: number[], id: number, checked: boolean): number[] {
  const without = ids.filter((existing) => existing !== id);
  return checked ? [...without, id] : without;
}
