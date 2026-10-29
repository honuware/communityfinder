import {
  pacificDayKey,
  pacificInputsToUs,
  pacificTimeOfDay,
} from '../../core/utils/events-date.utils';

// The event editor's form model, kept out of the page so the rules are
// tested on their own: Pacific wall-clock inputs → `_us` instants, an events
// row → the form and back, and the category / scene checkbox changes → the
// assignment rows to add and delete.

export interface EventFormValue {
  title: string;
  description: string;
  // <input type="date"> / <input type="time"> values: "2026-08-14", "22:00",
  // Pacific wall-clock time.
  startDate: string;
  startTime: string;
  // Optional. An end time alone is on the start day, or the next day when
  // it's earlier than the start (a 10 PM – 2 AM night).
  endDate: string;
  endTime: string;
  // FK ids as the generic CRUD sends them; '' for none.
  venueId: string;
  seriesId: string;
  locationText: string;
  url: string;
  costText: string;
}

export const EMPTY_EVENT_FORM: EventFormValue = {
  title: '',
  description: '',
  startDate: '',
  startTime: '',
  endDate: '',
  endTime: '',
  venueId: '',
  seriesId: '',
  locationText: '',
  url: '',
  costText: '',
};

export type ComposedTimes =
  { ok: true; startsAtUs: number; endsAtUs: number | null } | { ok: false; error: string };

const DAY_PATTERN = /^(\d{4})-(\d{2})-(\d{2})$/;

// "2026-08-14" → "2026-08-15", by calendar arithmetic.
function nextDay(dateText: string): string {
  const match = DAY_PATTERN.exec(dateText);
  if (!match) {
    return dateText;
  }
  const date = new Date(Date.UTC(Number(match[1]), Number(match[2]) - 1, Number(match[3]) + 1));
  return date.toISOString().slice(0, 10);
}

// The form's date and time inputs → the start and end instants.
export function composeEventTimes(value: EventFormValue): ComposedTimes {
  const startsAtUs = pacificInputsToUs(value.startDate, value.startTime);
  if (startsAtUs === null) {
    return { ok: false, error: 'Enter a start date and time.' };
  }
  if (!value.endTime.trim()) {
    return value.endDate.trim()
      ? { ok: false, error: 'Enter an end time too, or clear the end date.' }
      : { ok: true, startsAtUs, endsAtUs: null };
  }
  const endDate = value.endDate.trim() || value.startDate;
  let endsAtUs = pacificInputsToUs(endDate, value.endTime);
  if (endsAtUs === null) {
    return { ok: false, error: 'Enter a valid end date and time.' };
  }
  if (endsAtUs <= startsAtUs && !value.endDate.trim()) {
    endsAtUs = pacificInputsToUs(nextDay(value.startDate), value.endTime);
  }
  if (endsAtUs === null || endsAtUs <= startsAtUs) {
    return { ok: false, error: 'The end must be after the start.' };
  }
  return { ok: true, startsAtUs, endsAtUs };
}

// An `events` row (every value a string, as the generic table read sends it)
// → the form.
export function eventFormFromRow(row: Record<string, string>): EventFormValue {
  const startsAtUs = Number(row['starts_at_us']);
  const endsAtUs = row['ends_at_us'] ? Number(row['ends_at_us']) : null;
  return {
    title: row['title'] ?? '',
    description: row['description'] ?? '',
    startDate: startsAtUs ? pacificDayKey(startsAtUs) : '',
    startTime: startsAtUs ? pacificTimeOfDay(startsAtUs) : '',
    endDate: endsAtUs ? pacificDayKey(endsAtUs) : '',
    endTime: endsAtUs ? pacificTimeOfDay(endsAtUs) : '',
    venueId: row['venue_id'] ?? '',
    seriesId: row['series_id'] ?? '',
    locationText: row['location_text'] ?? '',
    url: row['url'] ?? '',
    costText: row['cost_text'] ?? '',
  };
}

// The `events` columns the form edits, as the generic CRUD writes them.
export type EventRowValue = Record<string, string>;

// Columns that are numbers in the database. A blank one is "none": left out
// of an insert (Postgres can't read '' as a number), and sent as '' on an
// update, which the server writes as NULL.
const NUMBER_COLUMNS = ['ends_at_us', 'venue_id', 'series_id'];

export function eventRowValue(
  value: EventFormValue,
  times: { startsAtUs: number; endsAtUs: number | null },
): EventRowValue {
  return {
    title: value.title.trim(),
    description: value.description.trim(),
    starts_at_us: String(times.startsAtUs),
    ends_at_us: times.endsAtUs === null ? '' : String(times.endsAtUs),
    venue_id: value.venueId,
    series_id: value.seriesId,
    location_text: value.locationText.trim(),
    url: value.url.trim(),
    cost_text: value.costText.trim(),
  };
}

// A new event's row: blank number columns left out, and pending, like any
// other new event (said outright rather than left to the column default, so
// the offline mock's queue shows it too). Origin and the timestamps take
// their defaults (manual, now).
export function rowForCreate(row: EventRowValue, createdByPersonId: number | null): EventRowValue {
  const insert: EventRowValue = {};
  for (const [column, value] of Object.entries(row)) {
    if (!(value === '' && NUMBER_COLUMNS.includes(column))) {
      insert[column] = value;
    }
  }
  insert['status'] = 'pending';
  if (createdByPersonId !== null) {
    insert['created_by_person_id'] = String(createdByPersonId);
  }
  return insert;
}

// An edit's changes: only the columns that differ from the row as loaded,
// so a save never rewrites what the curator didn't touch.
export function rowChanges(row: EventRowValue, original: Record<string, string>): EventRowValue {
  const changes: EventRowValue = {};
  for (const [column, value] of Object.entries(row)) {
    if ((original[column] ?? '') !== value) {
      changes[column] = value;
    }
  }
  return changes;
}

// One assignment row: an event's link to a category (or scene tag).
export interface AssignmentRow {
  // The assignment row's own id.
  id: string;
  // The category's or scene tag's id.
  tagId: number;
}

export interface AssignmentChanges {
  add: number[];
  // Assignment row ids.
  remove: string[];
}

// The checkbox state → the rows to add and delete. Only tags the form shows
// are touched: an assignment to a turned-off category (not on the form)
// stays as it is.
export function assignmentChanges(
  existing: AssignmentRow[],
  shownTagIds: number[],
  selectedTagIds: number[],
): AssignmentChanges {
  const selected = new Set(selectedTagIds);
  const shown = new Set(shownTagIds);
  const assigned = new Set(existing.map((row) => row.tagId));
  return {
    add: selectedTagIds.filter((tagId) => shown.has(tagId) && !assigned.has(tagId)),
    remove: existing
      .filter((row) => shown.has(row.tagId) && !selected.has(row.tagId))
      .map((row) => row.id),
  };
}
