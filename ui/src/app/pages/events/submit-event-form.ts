import {
  SubmissionRelationship,
  SubmissionRequest,
} from '../../core/event-submissions-access/event-submissions-access';
import { composeEventTimes, EventTimesValue } from '../admin/manage-events-form';

// The submit form's model (EV3 §5.2), kept out of the page so the rules are
// tested on their own: each date row's Seattle wall-clock inputs → `_us`
// instants (the event editor's rules: an end time earlier than the start,
// with no end date, is the next morning), and the form → the request body.
// The server checks everything again and has the last word; this catches
// only what the browser can know.

export const MAX_SUBMISSION_DATES = 12;

// One date row: its pickers' values as text ("2026-08-14", "22:00"), Pacific
// wall-clock time. The end is optional; an end date only when it ends on a
// later day (a festival).
export interface SubmitDateValue {
  date: string;
  startTime: string;
  endDate: string;
  endTime: string;
}

export const EMPTY_SUBMIT_DATE: SubmitDateValue = {
  date: '',
  startTime: '',
  endDate: '',
  endTime: '',
};

// A date row in the event editor's terms.
export function dateRowTimes(row: SubmitDateValue): EventTimesValue {
  return {
    startDate: row.date,
    startTime: row.startTime,
    endDate: row.endDate,
    endTime: row.endTime,
  };
}

export type PlaceMode = 'venue' | 'elsewhere';

export interface SubmitFormValue {
  title: string;
  dates: SubmitDateValue[];
  placeMode: PlaceMode;
  // A venue from the list; null for none yet.
  venueId: number | null;
  locationText: string;
  description: string;
  url: string;
  costText: string;
  categorySlugs: string[];
  sceneTagSlugs: string[];
  regularThing: boolean;
  relationship: SubmissionRelationship;
  submitterNote: string;
  agreed: boolean;
  // The honeypot.
  website: string;
}

export const EMPTY_SUBMIT_FORM: SubmitFormValue = {
  title: '',
  dates: [EMPTY_SUBMIT_DATE],
  placeMode: 'venue',
  venueId: null,
  locationText: '',
  description: '',
  url: '',
  costText: '',
  categorySlugs: [],
  sceneTagSlugs: [],
  regularThing: false,
  relationship: 'organizer',
  submitterNote: '',
  agreed: false,
  website: '',
};

// A problem the browser found, named the way the server names its own: the
// form field by its JSON name, and for a date, which one (from 0).
export interface SubmitFormProblem {
  field: string;
  dateIndex: number | null;
  message: string;
}

export type ComposedSubmission =
  { ok: true; request: SubmissionRequest } | ({ ok: false } & SubmitFormProblem);

// The form → the POST body, or the first problem, in the form's order.
// Optional text left blank is left out, and only the chosen kind of place is
// sent.
export function composeSubmission(value: SubmitFormValue): ComposedSubmission {
  const fail = (field: string, message: string, dateIndex: number | null = null) => ({
    ok: false as const,
    field,
    dateIndex,
    message,
  });
  if (!value.title.trim()) {
    return fail('title', 'Give the event a title.');
  }
  if (value.dates.length === 0) {
    return fail('dates', 'Add at least one date.');
  }
  if (value.dates.length > MAX_SUBMISSION_DATES) {
    return fail('dates', `A submission can have up to ${MAX_SUBMISSION_DATES} dates.`);
  }
  const dates: SubmissionRequest['dates'] = [];
  for (const [index, row] of value.dates.entries()) {
    const times = composeEventTimes(dateRowTimes(row));
    if (!times.ok) {
      return fail('dates', times.error, index);
    }
    dates.push(
      times.endsAtUs === null
        ? { starts_at_us: times.startsAtUs }
        : { starts_at_us: times.startsAtUs, ends_at_us: times.endsAtUs },
    );
  }
  if (value.placeMode === 'venue' && value.venueId === null) {
    return fail('venue_id', 'Pick a venue from the list, or choose "Somewhere else".');
  }
  if (value.placeMode === 'elsewhere' && !value.locationText.trim()) {
    return fail('location_text', 'Say where it is: a place name and address.');
  }
  if (!value.agreed) {
    return fail('agreed', 'Please confirm the details are accurate and can be listed publicly.');
  }

  const request: SubmissionRequest = {
    title: value.title.trim(),
    dates,
    agreed: true,
    relationship: value.relationship,
    regular_thing: value.regularThing,
  };
  if (value.placeMode === 'venue' && value.venueId !== null) {
    request.venue_id = value.venueId;
  }
  const text = (key: keyof SubmissionRequest, raw: string) => {
    if (raw.trim()) {
      (request as unknown as Record<string, unknown>)[key] = raw.trim();
    }
  };
  if (value.placeMode === 'elsewhere') {
    text('location_text', value.locationText);
  }
  text('description', value.description);
  text('url', value.url);
  text('cost_text', value.costText);
  text('submitter_note', value.submitterNote);
  if (value.categorySlugs.length) {
    request.category_slugs = [...value.categorySlugs];
  }
  if (value.sceneTagSlugs.length) {
    request.scene_tag_slugs = [...value.sceneTagSlugs];
  }
  if (value.website) {
    request.website = value.website;
  }
  return { ok: true, request };
}
