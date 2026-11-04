import { Injectable } from '@angular/core';
import {
  SubmissionLimitName,
  SubmissionSettings,
  SubmissionSettingsUpdate,
} from '../events-admin-access/events-admin-access';
import { SubmissionProblem } from '../event-submissions-access/event-submissions-access';

// The server's defaults and ranges (EV3 §3.3), the mock's own copy. Pages
// never read these: they take them from the settings GET.
const LIMITS: Record<SubmissionLimitName, { default: number; min: number; max: number }> = {
  max_pending_dates: { default: 10, min: 1, max: 100 },
  max_submissions_per_day: { default: 5, min: 1, max: 50 },
};

// The offline stand-in for the three settings the server keeps in
// config_secrets: both rate limits and the pause switch. One instance for the
// whole app, so pausing on the settings page closes the mock submit form too.
@Injectable({ providedIn: 'root' })
export class SubmissionSettingsMockStore {
  private values = {
    max_pending_dates: LIMITS.max_pending_dates.default,
    max_submissions_per_day: LIMITS.max_submissions_per_day.default,
    submissions_paused: false,
  };

  get maxPendingDates(): number {
    return this.values.max_pending_dates;
  }

  get maxSubmissionsPerDay(): number {
    return this.values.max_submissions_per_day;
  }

  get paused(): boolean {
    return this.values.submissions_paused;
  }

  settings(): SubmissionSettings {
    const limit = (name: SubmissionLimitName) => ({ value: this.values[name], ...LIMITS[name] });
    return {
      max_pending_dates: limit('max_pending_dates'),
      max_submissions_per_day: limit('max_submissions_per_day'),
      submissions_paused: { value: this.values.submissions_paused, default: false },
    };
  }

  // The server's all-or-nothing save: an out-of-range limit saves nothing and
  // is refused naming its field.
  save(update: SubmissionSettingsUpdate): SubmissionProblem | null {
    for (const name of Object.keys(LIMITS) as SubmissionLimitName[]) {
      const value = update[name];
      if (
        value !== undefined &&
        (!Number.isInteger(value) || value < LIMITS[name].min || value > LIMITS[name].max)
      ) {
        return {
          status: 400,
          error_code: 'LIMIT_OUT_OF_RANGE',
          field: name,
          date_index: null,
          detail: `${name} is outside its allowed range`,
        };
      }
    }
    this.values = {
      max_pending_dates: update.max_pending_dates ?? this.values.max_pending_dates,
      max_submissions_per_day:
        update.max_submissions_per_day ?? this.values.max_submissions_per_day,
      submissions_paused: update.submissions_paused ?? this.values.submissions_paused,
    };
    return null;
  }
}
