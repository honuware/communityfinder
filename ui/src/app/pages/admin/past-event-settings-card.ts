import { Component, computed, DestroyRef, inject, OnInit, signal } from '@angular/core';
import { takeUntilDestroyed } from '@angular/core/rxjs-interop';
import { FormControl, FormGroup, ReactiveFormsModule } from '@angular/forms';
import { MatButtonModule } from '@angular/material/button';
import { MatCardModule } from '@angular/material/card';
import { MatFormFieldModule } from '@angular/material/form-field';
import { MatInputModule } from '@angular/material/input';
import { MatProgressSpinnerModule } from '@angular/material/progress-spinner';
import { MatSlideToggleChange, MatSlideToggleModule } from '@angular/material/slide-toggle';
import { ToastService } from '@honuware/ui/foundation';
import {
  EVENTS_ADMIN_ACCESS,
  PastEventSettings,
  RangedSetting,
} from '../../core/events-admin-access/events-admin-access';
import { toSubmissionProblem } from '../../core/event-submissions-access/event-submissions-access';

// The Past events card on the Event settings page (EV4 §10.4): how long an
// event stays on the site after it ends, and whether the calendar shows past
// events before a visitor chooses. Both apply to the very next page view.
//
// - The window shows its default and allowed range, as the server sends them.
//   Save sends it only when it changed; "Reset to default" fills in the
//   default for a Save to apply.
// - The calendar switch saves on its own, like the submissions pause switch.
@Component({
  selector: 'app-past-event-settings-card',
  imports: [
    ReactiveFormsModule,
    MatButtonModule,
    MatCardModule,
    MatFormFieldModule,
    MatInputModule,
    MatProgressSpinnerModule,
    MatSlideToggleModule,
  ],
  templateUrl: './past-event-settings-card.html',
  styleUrl: './past-event-settings-card.scss',
})
export class PastEventSettingsCard implements OnInit {
  private access = inject(EVENTS_ADMIN_ACCESS);
  private toast = inject(ToastService);
  private destroyRef = inject(DestroyRef);

  readonly loading = signal(true);
  readonly failed = signal(false);
  // The settings as last loaded or saved.
  readonly settings = signal<PastEventSettings | null>(null);
  readonly saving = signal(false);
  readonly savingSwitch = signal(false);
  readonly fieldError = signal('');
  readonly formError = signal('');

  readonly form = new FormGroup({ visibleDays: new FormControl<number | null>(null) });
  private readonly entered = signal<number | null>(null);

  readonly range = computed<RangedSetting | null>(
    () => this.settings()?.archived_events_visible_days ?? null,
  );
  readonly showByDefault = computed(
    () => this.settings()?.events_show_past_by_default.value ?? false,
  );
  readonly changed = computed(() => {
    const range = this.range();
    return range !== null && this.entered() !== range.value;
  });

  ngOnInit(): void {
    this.form.controls.visibleDays.valueChanges
      .pipe(takeUntilDestroyed(this.destroyRef))
      .subscribe((value) => this.entered.set(value));
    this.access
      .getPastEventSettings()
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

  resetToDefault(): void {
    const range = this.range();
    if (range) {
      this.form.controls.visibleDays.setValue(range.default);
      this.fieldError.set('');
    }
  }

  save(): void {
    this.fieldError.set('');
    this.formError.set('');
    const range = this.range();
    if (!range || !this.changed()) {
      return;
    }
    const entered = this.entered();
    const days = entered === null ? Number.NaN : Number(entered);
    if (!inRange(days, range)) {
      this.fieldError.set(rangeMessage(range));
      return;
    }
    this.saving.set(true);
    this.access
      .savePastEventSettings({ archived_events_visible_days: days })
      .pipe(takeUntilDestroyed(this.destroyRef))
      .subscribe({
        next: (settings) => {
          this.saving.set(false);
          this.showSaved(settings);
          this.toast.success(
            `Saved. An event now stays on the site for ${days} ${days === 1 ? 'day' : 'days'} after it ends.`,
          );
        },
        error: (error: unknown) => {
          this.saving.set(false);
          const problem = toSubmissionProblem(error);
          if (problem.field === 'archived_events_visible_days') {
            this.fieldError.set(rangeMessage(range));
          } else {
            this.formError.set(problem.detail);
          }
        },
      });
  }

  saveSwitch(change: MatSlideToggleChange): void {
    const show = change.checked;
    this.savingSwitch.set(true);
    this.access
      .savePastEventSettings({ events_show_past_by_default: show })
      .pipe(takeUntilDestroyed(this.destroyRef))
      .subscribe({
        next: (saved) => {
          this.savingSwitch.set(false);
          // Only the switch: a window being edited stays as typed.
          this.settings.update((settings) =>
            settings
              ? { ...settings, events_show_past_by_default: saved.events_show_past_by_default }
              : saved,
          );
          this.toast.success(
            show
              ? 'The calendar now shows past events until a visitor turns them off.'
              : 'The calendar now hides past events until a visitor turns them on.',
          );
        },
        error: () => {
          this.savingSwitch.set(false);
          change.source.checked = !show;
          this.toast.error("The switch couldn't be saved. Please try again.");
        },
      });
  }

  private showSaved(settings: PastEventSettings): void {
    this.settings.set(settings);
    this.form.controls.visibleDays.setValue(settings.archived_events_visible_days.value);
  }
}

function inRange(value: number, range: RangedSetting): boolean {
  return Number.isInteger(value) && value >= range.min && value <= range.max;
}

function rangeMessage(range: RangedSetting): string {
  return `Enter a whole number from ${range.min} to ${range.max}.`;
}
