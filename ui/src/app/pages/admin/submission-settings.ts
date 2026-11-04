import { Component, computed, DestroyRef, inject, OnInit, signal } from '@angular/core';
import { takeUntilDestroyed } from '@angular/core/rxjs-interop';
import { FormControl, FormGroup, ReactiveFormsModule } from '@angular/forms';
import { RouterLink } from '@angular/router';
import { MatButtonModule } from '@angular/material/button';
import { MatCardModule } from '@angular/material/card';
import { MatFormFieldModule } from '@angular/material/form-field';
import { MatIconModule } from '@angular/material/icon';
import { MatInputModule } from '@angular/material/input';
import { MatProgressSpinnerModule } from '@angular/material/progress-spinner';
import { MatSlideToggleChange, MatSlideToggleModule } from '@angular/material/slide-toggle';
import { ToastService } from '@honuware/ui/foundation';
import {
  EVENTS_ADMIN_ACCESS,
  SubmissionLimitName,
  SubmissionLimitSetting,
  SubmissionSettings,
  SubmissionSettingsUpdate,
} from '../../core/events-admin-access/events-admin-access';
import { toSubmissionProblem } from '../../core/event-submissions-access/event-submissions-access';
import { PastEventSettingsCard } from './past-event-settings-card';

interface LimitField {
  name: SubmissionLimitName;
  control: 'maxPendingDates' | 'maxSubmissionsPerDay';
  label: string;
  explanation: string;
}

export const LIMIT_FIELDS: ReadonlyArray<LimitField> = [
  {
    name: 'max_pending_dates',
    control: 'maxPendingDates',
    label: 'Dates one person can have waiting for review',
    explanation:
      'Counted across all of their submissions. A twelve-date submission needs room for twelve.',
  },
  {
    name: 'max_submissions_per_day',
    control: 'maxSubmissionsPerDay',
    label: 'Submissions one person can send in 24 hours',
    explanation: 'Each submission counts once, however many dates it has.',
  },
];

// The Event settings page (/admin/events/settings, EV3 §5.6), for anyone who
// can review events. The rate limits and the pause switch take effect on the
// very next submission; no deploy needed. The past-event settings (EV4 §10.4)
// are their own card below, with their own load and save.
//
// - The switch at the top saves on its own. Pausing asks first, in the page;
//   turning submissions back on doesn't.
// - Each limit shows its default and allowed range, as the server sends them.
//   Save sends only what changed; "Reset to defaults" fills in the defaults
//   (the limits only, never the switch) for a Save to apply.
@Component({
  selector: 'app-submission-settings',
  imports: [
    RouterLink,
    ReactiveFormsModule,
    MatButtonModule,
    MatCardModule,
    MatFormFieldModule,
    MatIconModule,
    MatInputModule,
    MatProgressSpinnerModule,
    MatSlideToggleModule,
    PastEventSettingsCard,
  ],
  templateUrl: './submission-settings.html',
  styleUrl: './submission-settings.scss',
})
export class SubmissionSettingsPage implements OnInit {
  private access = inject(EVENTS_ADMIN_ACCESS);
  private toast = inject(ToastService);
  private destroyRef = inject(DestroyRef);

  readonly limitFields = LIMIT_FIELDS;

  readonly loading = signal(true);
  readonly failed = signal(false);
  // The settings as last loaded or saved.
  readonly settings = signal<SubmissionSettings | null>(null);
  readonly saving = signal(false);
  readonly savingSwitch = signal(false);
  readonly confirmingPause = signal(false);
  readonly fieldErrors = signal<Partial<Record<SubmissionLimitName, string>>>({});
  readonly formError = signal('');

  readonly form = new FormGroup({
    maxPendingDates: new FormControl<number | null>(null),
    maxSubmissionsPerDay: new FormControl<number | null>(null),
  });
  private readonly formValue = signal(this.form.getRawValue());

  readonly paused = computed(() => this.settings()?.submissions_paused.value ?? false);
  readonly changed = computed(() => Object.keys(this.changes()).length > 0);

  ngOnInit(): void {
    this.form.valueChanges
      .pipe(takeUntilDestroyed(this.destroyRef))
      .subscribe(() => this.formValue.set(this.form.getRawValue()));
    this.access
      .getSubmissionSettings()
      .pipe(takeUntilDestroyed(this.destroyRef))
      .subscribe({
        next: (settings) => {
          this.showSaved(settings);
          this.loading.set(false);
        },
        error: () => {
          this.failed.set(true);
          this.loading.set(false);
        },
      });
  }

  limit(name: SubmissionLimitName): SubmissionLimitSetting | null {
    return this.settings()?.[name] ?? null;
  }

  fieldError(name: SubmissionLimitName): string {
    return this.fieldErrors()[name] ?? '';
  }

  // The switch reads "Taking submissions" when on. Turning it off asks
  // first, and the switch stays on until that's confirmed.
  onSwitchChange(change: MatSlideToggleChange): void {
    if (change.checked) {
      this.saveSwitch(false);
      return;
    }
    change.source.checked = true;
    this.confirmingPause.set(true);
  }

  confirmPause(): void {
    this.confirmingPause.set(false);
    this.saveSwitch(true);
  }

  cancelPause(): void {
    this.confirmingPause.set(false);
  }

  resetToDefaults(): void {
    const settings = this.settings();
    if (!settings) {
      return;
    }
    this.form.setValue({
      maxPendingDates: settings.max_pending_dates.default,
      maxSubmissionsPerDay: settings.max_submissions_per_day.default,
    });
    this.fieldErrors.set({});
  }

  save(): void {
    this.fieldErrors.set({});
    this.formError.set('');
    const update = this.changes();
    for (const field of LIMIT_FIELDS) {
      const value = update[field.name];
      const range = this.limit(field.name);
      if (value !== undefined && range && !inRange(value, range)) {
        this.fieldErrors.set({ [field.name]: rangeMessage(range) });
        return;
      }
    }
    if (Object.keys(update).length === 0) {
      return;
    }
    this.saving.set(true);
    this.access
      .saveSubmissionSettings(update)
      .pipe(takeUntilDestroyed(this.destroyRef))
      .subscribe({
        next: (settings) => {
          this.saving.set(false);
          this.showSaved(settings);
          this.toast.success('Saved. The new limits apply to the next submission.');
        },
        error: (error: unknown) => {
          this.saving.set(false);
          const problem = toSubmissionProblem(error);
          const range = isLimitName(problem.field) ? this.limit(problem.field) : null;
          if (isLimitName(problem.field) && range) {
            this.fieldErrors.set({ [problem.field]: rangeMessage(range) });
          } else {
            this.formError.set(problem.detail);
          }
        },
      });
  }

  // The limits that differ from what's saved. A blank field counts as
  // changed, so Save can say what's wrong with it.
  private changes(): SubmissionSettingsUpdate {
    const settings = this.settings();
    const value = this.formValue();
    const update: SubmissionSettingsUpdate = {};
    if (!settings) {
      return update;
    }
    for (const field of LIMIT_FIELDS) {
      const entered = value[field.control];
      if (entered !== settings[field.name].value) {
        update[field.name] = entered === null ? Number.NaN : Number(entered);
      }
    }
    return update;
  }

  private saveSwitch(paused: boolean): void {
    this.savingSwitch.set(true);
    this.access
      .saveSubmissionSettings({ submissions_paused: paused })
      .pipe(takeUntilDestroyed(this.destroyRef))
      .subscribe({
        next: (saved) => {
          this.savingSwitch.set(false);
          // Only the switch: any limit being edited stays as typed.
          this.settings.update((settings) =>
            settings ? { ...settings, submissions_paused: saved.submissions_paused } : saved,
          );
          this.toast.success(
            paused
              ? 'Submissions are paused. The form shows a notice instead.'
              : 'Submissions are open again.',
          );
        },
        error: () => {
          this.savingSwitch.set(false);
          this.toast.error("The switch couldn't be saved. Please try again.");
        },
      });
  }

  private showSaved(settings: SubmissionSettings): void {
    this.settings.set(settings);
    this.form.setValue({
      maxPendingDates: settings.max_pending_dates.value,
      maxSubmissionsPerDay: settings.max_submissions_per_day.value,
    });
  }
}

function inRange(value: number, range: SubmissionLimitSetting): boolean {
  return Number.isInteger(value) && value >= range.min && value <= range.max;
}

function rangeMessage(range: SubmissionLimitSetting): string {
  return `Enter a whole number from ${range.min} to ${range.max}.`;
}

function isLimitName(field: string): field is SubmissionLimitName {
  return LIMIT_FIELDS.some((limit) => limit.name === field);
}
