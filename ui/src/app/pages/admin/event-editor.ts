import {
  Component,
  DestroyRef,
  effect,
  inject,
  input,
  output,
  signal,
  untracked,
} from '@angular/core';
import { takeUntilDestroyed } from '@angular/core/rxjs-interop';
import { FormControl, FormGroup, ReactiveFormsModule, Validators } from '@angular/forms';
import { RouterLink } from '@angular/router';
import { concat, forkJoin, last, map, Observable, of, switchMap } from 'rxjs';
import { MatButtonModule } from '@angular/material/button';
import { MatCheckboxModule } from '@angular/material/checkbox';
import { provideNativeDateAdapter } from '@angular/material/core';
import { MAT_FORM_FIELD_DEFAULT_OPTIONS, MatFormFieldModule } from '@angular/material/form-field';
import { MatInputModule } from '@angular/material/input';
import { MatProgressSpinnerModule } from '@angular/material/progress-spinner';
import { MatSelectModule } from '@angular/material/select';
import { DataResults, FkOption, HONUWARE_CRUD_ACCESS } from '@honuware/ui/access';
import { AuthService } from '@honuware/ui/auth';
import { SimpleDateComponent } from '@honuware/ui/controls';
import { ToastService } from '@honuware/ui/foundation';
import { dataResultsToRecords } from '../../core/types/events.types';
import {
  datePickerValue,
  pickedDateText,
  pickedTimeText,
  pickerField,
  timePickerValue,
} from '../../core/utils/date-picker-values';
import { eventPagePath } from '../../core/utils/event-links';
import {
  AssignmentRow,
  assignmentChanges,
  composeEventTimes,
  EMPTY_EVENT_FORM,
  eventFormFromRow,
  EventFormValue,
  eventRowValue,
  nextDayEndNote,
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

// Enough rows for any one event's tags.
const PAGE_SIZE = 500;

// A category or scene the form offers as a checkbox.
export interface TagChoice {
  id: number;
  label: string;
}

// What the Edit Event tab asks its editor to open. Each request is a new
// object, so asking for the same event again reloads it.
export interface EditRequest {
  id: string;
}

// What a save tells the page: the event, and whether it was just added.
export interface SavedEvent {
  id: string;
  added: boolean;
}

// What the date and time pickers show when they're made. A picker takes a
// value only then (feeding it each change would rewrite the box while it's
// typed in), so each load makes a new one with the next `load` number, and
// the template remakes the pickers from it.
export interface PickerStarts {
  load: number;
  startDate: string;
  startTime: string;
  endDate: string;
  endTime: string;
}

function pickerStartsFor(value: EventFormValue, load: number): PickerStarts {
  return {
    load,
    startDate: datePickerValue(value.startDate),
    startTime: timePickerValue(value.startTime),
    endDate: datePickerValue(value.endDate),
    endTime: timePickerValue(value.endTime),
  };
}

// One event's form (EV2 §3.7), on the events page's New Event and Edit Event
// tabs. With no request it's a new event; given one, it loads that event and
// saves only what changed. Like ManageEventsPage, it talks to the generic
// HONUWARE_CRUD_ACCESS directly.
//
// - Dates and times use the honuware pickers. They're Seattle wall-clock
//   time, stored as `_us` instants.
// - Category and scene checkboxes write the two assignment tables. Left all
//   unchecked, an event in a series shows the series' own tags (EV1's
//   inheritance rule).
// - A new event is saved as pending, like any other; it goes public once
//   approved in the queue.
@Component({
  selector: 'app-event-editor',
  imports: [
    RouterLink,
    ReactiveFormsModule,
    MatButtonModule,
    MatCheckboxModule,
    MatFormFieldModule,
    MatInputModule,
    MatProgressSpinnerModule,
    MatSelectModule,
    SimpleDateComponent,
  ],
  providers: [
    // The honuware pickers' own date-adapter import doesn't reach them inside
    // another component's template, so the editor provides one.
    provideNativeDateAdapter(),
    // They draw their own form fields; this gives them the form's outline look.
    { provide: MAT_FORM_FIELD_DEFAULT_OPTIONS, useValue: { appearance: 'outline' } },
  ],
  templateUrl: './event-editor.html',
  styleUrl: './event-editor.scss',
})
export class EventEditor {
  private crud = inject(HONUWARE_CRUD_ACCESS);
  private auth = inject(AuthService);
  private toast = inject(ToastService);
  private destroyRef = inject(DestroyRef);

  // The event to edit; null for a new one.
  readonly request = input<EditRequest | null>(null);
  readonly venues = input<FkOption[]>([]);
  readonly series = input<FkOption[]>([]);
  readonly categories = input<TagChoice[]>([]);
  readonly sceneTags = input<TagChoice[]>([]);
  readonly saved = output<SavedEvent>();

  readonly startPickerField = pickerField('Start', true);
  readonly endPickerField = pickerField('End', false);
  readonly pickerStarts = signal<PickerStarts>(pickerStartsFor(EMPTY_EVENT_FORM, 0));

  readonly selectedCategoryIds = signal<number[]>([]);
  readonly selectedSceneTagIds = signal<number[]>([]);

  // The event as loaded; null for a new one, or while one loads.
  readonly editingId = signal<string | null>(null);
  readonly editingTitle = signal('');
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

  constructor() {
    effect(() => {
      const request = this.request();
      untracked(() => (request ? this.open(request.id) : this.clear()));
    });
  }

  // A picker's value → its form control, on a pick and as it's typed in.
  // The picker announces only readable values, so typing (which can clear the
  // box) reads the picker's own value instead.
  syncPicked(control: FormControl<string>, picker: SimpleDateComponent): void {
    const value = picker.dateControl.value;
    control.setValue(picker.type === 'time' ? pickedTimeText(value) : pickedDateText(value));
  }

  endNote(): string {
    return nextDayEndNote(this.form.getRawValue());
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

  previewPath(id: string): string {
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
          if (editingId) {
            this.toast.success('Event saved.');
            this.load(eventId);
          } else {
            this.toast.success('Event added. It goes public once approved in the queue.');
            this.clear();
          }
          this.saved.emit({ id: eventId, added: !editingId });
        },
        error: () => {
          this.saving.set(false);
          this.formError.set("The event couldn't be saved. Check the fields and try again.");
        },
      });
  }

  // A blank form, for a new event.
  private clear(): void {
    this.editingId.set(null);
    this.editingTitle.set('');
    this.original = {};
    this.originalCategories = [];
    this.originalSceneTags = [];
    this.fill(EMPTY_EVENT_FORM, [], []);
    this.formError.set('');
  }

  // Shows only the loading (or the problem) until the event is in, so a form
  // that failed to load can't be saved as a new event.
  private open(id: string): void {
    this.editingId.set(null);
    this.formError.set('');
    this.load(id);
  }

  private load(id: string): void {
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
          this.original = record;
          this.originalCategories = categories;
          this.originalSceneTags = sceneTags;
          this.fill(eventFormFromRow(record), categories, sceneTags);
          this.editingTitle.set(record['title'] ?? '');
          this.editingId.set(id);
        },
        error: () => this.formError.set(`Event ${id} couldn't be loaded.`),
      });
  }

  private fill(
    value: EventFormValue,
    categories: AssignmentRow[],
    sceneTags: AssignmentRow[],
  ): void {
    this.form.reset(value);
    this.pickerStarts.update((starts) => pickerStartsFor(value, starts.load + 1));
    this.selectedCategoryIds.set(categories.map((row) => row.tagId));
    this.selectedSceneTagIds.set(sceneTags.map((row) => row.tagId));
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
