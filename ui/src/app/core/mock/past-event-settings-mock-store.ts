import { Injectable } from '@angular/core';
import {
  PastEventSettings,
  PastEventSettingsUpdate,
} from '../events-admin-access/events-admin-access';
import { SubmissionProblem } from '../event-submissions-access/event-submissions-access';
import { DEFAULT_EVENT_SETTINGS, EventSettings } from '../types/events.types';

// The server's range for the window (EV4 §10.3), the mock's own copy. Pages
// never read it: they take it from the settings GET.
const VISIBLE_DAYS = {
  default: DEFAULT_EVENT_SETTINGS.archived_events_visible_days,
  min: 1,
  max: 730,
};

// The offline stand-in for the two past-event settings the server keeps in
// config_secrets. One instance for the whole app, so a shorter window saved on
// the settings page hides older events on the mock's public pages too.
@Injectable({ providedIn: 'root' })
export class PastEventSettingsMockStore {
  private values: EventSettings = { ...DEFAULT_EVENT_SETTINGS };

  get visibleDays(): number {
    return this.values.archived_events_visible_days;
  }

  // What GET /api/events/settings serves.
  publicSettings(): EventSettings {
    return { ...this.values };
  }

  settings(): PastEventSettings {
    return {
      archived_events_visible_days: {
        value: this.values.archived_events_visible_days,
        ...VISIBLE_DAYS,
      },
      events_show_past_by_default: {
        value: this.values.events_show_past_by_default,
        default: DEFAULT_EVENT_SETTINGS.events_show_past_by_default,
      },
    };
  }

  // The server's all-or-nothing save: an out-of-range window saves nothing.
  save(update: PastEventSettingsUpdate): SubmissionProblem | null {
    const days = update.archived_events_visible_days;
    if (
      days !== undefined &&
      (!Number.isInteger(days) || days < VISIBLE_DAYS.min || days > VISIBLE_DAYS.max)
    ) {
      return {
        status: 400,
        error_code: 'DAYS_OUT_OF_RANGE',
        field: 'archived_events_visible_days',
        date_index: null,
        detail: 'archived_events_visible_days is outside its allowed range',
      };
    }
    this.values = {
      archived_events_visible_days: days ?? this.values.archived_events_visible_days,
      events_show_past_by_default:
        update.events_show_past_by_default ?? this.values.events_show_past_by_default,
    };
    return null;
  }
}
