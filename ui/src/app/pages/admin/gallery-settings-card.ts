import { Component, computed, DestroyRef, inject, OnInit, signal } from '@angular/core';
import { takeUntilDestroyed } from '@angular/core/rxjs-interop';
import { FormControl, FormGroup, ReactiveFormsModule } from '@angular/forms';
import { MatButtonModule } from '@angular/material/button';
import { MatCardModule } from '@angular/material/card';
import { MatFormFieldModule } from '@angular/material/form-field';
import { MatInputModule } from '@angular/material/input';
import { MatProgressSpinnerModule } from '@angular/material/progress-spinner';
import { ToastService } from '@honuware/ui/foundation';
import {
  EVENTS_ADMIN_ACCESS,
  GallerySettings,
  RangedSetting,
} from '../../core/events-admin-access/events-admin-access';
import { toSubmissionProblem } from '../../core/event-submissions-access/event-submissions-access';

// The Photo galleries card on the Event settings page (CM3 §5.6, OQ5): how
// many photos one event's or series' gallery may hold. It shows its default
// and allowed range as the server sends them; Save sends it only when it
// changed, and "Reset to default" fills in the default for a Save to apply.
// Lowering it never removes photos, only stops new ones.
@Component({
  selector: 'app-gallery-settings-card',
  imports: [
    ReactiveFormsModule,
    MatButtonModule,
    MatCardModule,
    MatFormFieldModule,
    MatInputModule,
    MatProgressSpinnerModule,
  ],
  templateUrl: './gallery-settings-card.html',
  styleUrl: './gallery-settings-card.scss',
})
export class GallerySettingsCard implements OnInit {
  private access = inject(EVENTS_ADMIN_ACCESS);
  private toast = inject(ToastService);
  private destroyRef = inject(DestroyRef);

  readonly loading = signal(true);
  readonly failed = signal(false);
  // The setting as last loaded or saved.
  readonly settings = signal<GallerySettings | null>(null);
  readonly saving = signal(false);
  readonly fieldError = signal('');
  readonly formError = signal('');

  readonly form = new FormGroup({ maxPhotos: new FormControl<number | null>(null) });
  private readonly entered = signal<number | null>(null);

  readonly range = computed<RangedSetting | null>(
    () => this.settings()?.event_gallery_max_photos ?? null,
  );
  readonly changed = computed(() => {
    const range = this.range();
    return range !== null && this.entered() !== range.value;
  });

  ngOnInit(): void {
    this.form.controls.maxPhotos.valueChanges
      .pipe(takeUntilDestroyed(this.destroyRef))
      .subscribe((value) => this.entered.set(value));
    this.access
      .getGallerySettings()
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
      this.form.controls.maxPhotos.setValue(range.default);
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
    const maxPhotos = entered === null ? Number.NaN : Number(entered);
    if (!Number.isInteger(maxPhotos) || maxPhotos < range.min || maxPhotos > range.max) {
      this.fieldError.set(rangeMessage(range));
      return;
    }
    this.saving.set(true);
    this.access
      .saveGallerySettings(maxPhotos)
      .pipe(takeUntilDestroyed(this.destroyRef))
      .subscribe({
        next: (settings) => {
          this.saving.set(false);
          this.showSaved(settings);
          this.toast.success(
            `Saved. A gallery can now hold up to ${maxPhotos} ${maxPhotos === 1 ? 'photo' : 'photos'}.`,
          );
        },
        error: (error: unknown) => {
          this.saving.set(false);
          const problem = toSubmissionProblem(error);
          if (problem.field === 'event_gallery_max_photos') {
            this.fieldError.set(rangeMessage(range));
          } else {
            this.formError.set(problem.detail);
          }
        },
      });
  }

  private showSaved(settings: GallerySettings): void {
    this.settings.set(settings);
    this.form.controls.maxPhotos.setValue(settings.event_gallery_max_photos.value);
  }
}

function rangeMessage(range: RangedSetting): string {
  return `Enter a whole number from ${range.min} to ${range.max}.`;
}
