import {
  Component,
  DestroyRef,
  effect,
  inject,
  input,
  signal,
  untracked,
  viewChild,
} from '@angular/core';
import { takeUntilDestroyed } from '@angular/core/rxjs-interop';
import { FormControl, FormGroup, ReactiveFormsModule } from '@angular/forms';
import { catchError, concat, last, map, Observable, of, switchMap, throwError } from 'rxjs';
import { MatButtonModule } from '@angular/material/button';
import { MatCheckboxModule } from '@angular/material/checkbox';
import { MatFormFieldModule } from '@angular/material/form-field';
import { MatIconModule } from '@angular/material/icon';
import { MatInputModule } from '@angular/material/input';
import { MatProgressSpinnerModule } from '@angular/material/progress-spinner';
import { HONUWARE_CRUD_ACCESS, HONUWARE_PHOTO_ACCESS } from '@honuware/ui/access';
import { ToastService } from '@honuware/ui/foundation';
import { PhotoUploadComponent } from '@honuware/ui/photos';
import {
  EVENTS_ADMIN_ACCESS,
  GALLERY_ALT_TEXT_MAX,
  GALLERY_CAPTION_MAX,
  GALLERY_CREDIT_MAX,
  galleryProblemMessage,
  toGalleryProblem,
} from '../../core/events-admin-access/events-admin-access';
import { EventPhotos, GALLERY_THUMB_SIZE } from '../../core/services/event-photos.service';
import { dataResultsToRecords } from '../../core/types/events.types';
import { PhotoPolicyNote } from './photo-policy-note';

const GALLERY_TABLE = 'event_gallery_photos';
// More than any cap allows (60).
const PAGE_SIZE = 500;

// One gallery row as the editor shows it.
export interface GalleryRow {
  id: number;
  sortOrder: number;
  altText: string;
  caption: string;
  credit: string;
}

// Thrown when the row was made but its photo didn't go up (and the row was
// taken back out), so the message can say so.
const UPLOAD_FAILED = { uploadFailed: true } as const;

// The Gallery section of the event editor (CM3 §5.6), for an event that's
// been saved. Its photos in order, each with its alt text, caption and
// credit; move one up or down (generic CRUD rewrites sort_order); remove one
// after a confirm in the page (its photo, then its row). "Add photo" takes
// the alt text, an optional caption and credit, the consent box, and a photo
// held back (hw-photo-upload's deferUpload): the row is made first through
// POST /api/admin/event_gallery_photos, which stamps the consent record and
// enforces the cap, then the photo goes up against the new row. If the
// upload fails the row is taken back out, so a gallery never lists a row with
// no picture.
@Component({
  selector: 'app-event-gallery-editor',
  imports: [
    ReactiveFormsModule,
    MatButtonModule,
    MatCheckboxModule,
    MatFormFieldModule,
    MatIconModule,
    MatInputModule,
    MatProgressSpinnerModule,
    PhotoPolicyNote,
    PhotoUploadComponent,
  ],
  templateUrl: './event-gallery-editor.html',
  styleUrl: './event-gallery-editor.scss',
})
export class EventGalleryEditor {
  private crud = inject(HONUWARE_CRUD_ACCESS);
  private photoAccess = inject(HONUWARE_PHOTO_ACCESS);
  private eventsAdmin = inject(EVENTS_ADMIN_ACCESS);
  private photos = inject(EventPhotos);
  private toast = inject(ToastService);
  private destroyRef = inject(DestroyRef);

  readonly eventId = input.required<number>();

  readonly loading = signal(true);
  readonly failed = signal(false);
  readonly rows = signal<GalleryRow[]>([]);
  // The cap, for "3 of 12"; null until (or unless) it's read.
  readonly maxPhotos = signal<number | null>(null);
  // A reorder or a removal on its way.
  readonly busy = signal(false);
  readonly confirmingRemoveId = signal<number | null>(null);
  readonly adding = signal(false);
  readonly addError = signal('');

  // Limits as the template's maxlength reads them.
  readonly altTextMax = GALLERY_ALT_TEXT_MAX;
  readonly captionMax = GALLERY_CAPTION_MAX;
  readonly creditMax = GALLERY_CREDIT_MAX;

  readonly addForm = new FormGroup({
    altText: new FormControl('', { nonNullable: true }),
    caption: new FormControl('', { nonNullable: true }),
    credit: new FormControl('', { nonNullable: true }),
    consent: new FormControl(false, { nonNullable: true }),
  });

  private readonly upload = viewChild(PhotoUploadComponent);

  constructor() {
    effect(() => {
      const eventId = this.eventId();
      untracked(() => {
        this.confirmingRemoveId.set(null);
        this.addError.set('');
        this.load(eventId);
      });
    });
    this.eventsAdmin
      .getGallerySettings()
      .pipe(takeUntilDestroyed(this.destroyRef))
      .subscribe({
        next: (settings) => this.maxPhotos.set(settings.event_gallery_max_photos.value),
        error: () => this.maxPhotos.set(null),
      });
  }

  thumbUrl(row: GalleryRow): string {
    return this.photos.galleryPhotoUrl(row, GALLERY_THUMB_SIZE);
  }

  move(index: number, by: -1 | 1): void {
    const rows = this.rows();
    const target = index + by;
    if (this.busy() || target < 0 || target >= rows.length) {
      return;
    }
    const reordered = [...rows];
    [reordered[index], reordered[target]] = [reordered[target], reordered[index]];
    // The two swap sort_orders. If any two rows share one (rows added at the
    // same moment), every row's becomes its place instead, so the order
    // holds. Only the ones that change are written.
    const distinct = new Set(rows.map((row) => row.sortOrder)).size === rows.length;
    const sortOrders = new Map<number, number>(
      distinct
        ? [
            [rows[index].id, rows[target].sortOrder],
            [rows[target].id, rows[index].sortOrder],
          ]
        : reordered.map((row, place) => [row.id, place]),
    );
    const moved = reordered.map((row) => ({
      ...row,
      sortOrder: sortOrders.get(row.id) ?? row.sortOrder,
    }));
    const writes = moved
      .filter((row, place) => row.sortOrder !== reordered[place].sortOrder)
      .map((row) =>
        this.crud.updateItem({
          table_name: GALLERY_TABLE,
          column_name: 'id',
          column_value: String(row.id),
          value: { sort_order: String(row.sortOrder) },
        }),
      );
    this.busy.set(true);
    (writes.length ? concat(...writes).pipe(last()) : of(undefined))
      .pipe(takeUntilDestroyed(this.destroyRef))
      .subscribe({
        next: () => {
          this.busy.set(false);
          this.rows.set(moved);
        },
        error: () => {
          this.busy.set(false);
          this.toast.error("The new order couldn't be saved. Please try again.");
          this.load(this.eventId());
        },
      });
  }

  askToRemove(row: GalleryRow): void {
    this.confirmingRemoveId.set(row.id);
  }

  cancelRemove(): void {
    this.confirmingRemoveId.set(null);
  }

  // Its photo first (a row with no photo has nothing to delete), then the row.
  remove(row: GalleryRow): void {
    this.confirmingRemoveId.set(null);
    this.busy.set(true);
    this.photoAccess
      .deletePhoto(GALLERY_TABLE, row.id)
      .pipe(
        catchError(() => of(undefined)),
        switchMap(() => this.crud.deleteItem(GALLERY_TABLE, 'id', String(row.id))),
        takeUntilDestroyed(this.destroyRef),
      )
      .subscribe({
        next: () => {
          this.busy.set(false);
          this.rows.update((rows) => rows.filter((candidate) => candidate.id !== row.id));
          this.toast.success('Photo removed.');
        },
        error: () => {
          this.busy.set(false);
          this.toast.error("The photo couldn't be removed. Please try again.");
          this.load(this.eventId());
        },
      });
  }

  add(): void {
    this.addError.set('');
    const value = this.addForm.getRawValue();
    const upload = this.upload();
    const problem = !value.altText.trim()
      ? 'Describe the photo for people who can’t see it.'
      : !value.consent
        ? 'Confirm that everyone recognizable in the photo agreed to be shown.'
        : !upload?.hasPendingFile
          ? 'Choose the photo to add.'
          : '';
    if (problem) {
      this.addError.set(problem);
      return;
    }
    this.adding.set(true);
    this.eventsAdmin
      .addGalleryPhoto({
        event_id: this.eventId(),
        alt_text: value.altText.trim(),
        caption: value.caption.trim(),
        credit: value.credit.trim(),
        consent_confirmed: true,
      })
      .pipe(
        switchMap(({ id }) => this.uploadOrTakeBack(upload!, id)),
        takeUntilDestroyed(this.destroyRef),
      )
      .subscribe({
        next: () => {
          this.adding.set(false);
          this.addForm.reset();
          upload!.clearPendingFile();
          this.toast.success('Photo added.');
          this.load(this.eventId());
        },
        error: (error: unknown) => {
          this.adding.set(false);
          this.addError.set(
            error === UPLOAD_FAILED
              ? "The photo couldn't be uploaded, so nothing was added. Try a JPEG or PNG."
              : galleryProblemMessage(toGalleryProblem(error)),
          );
        },
      });
  }

  private uploadOrTakeBack(upload: PhotoUploadComponent, id: number): Observable<void> {
    return upload.uploadPendingPhoto(GALLERY_TABLE, id).pipe(
      catchError(() =>
        this.crud.deleteItem(GALLERY_TABLE, 'id', String(id)).pipe(
          catchError(() => of(undefined)),
          switchMap(() => throwError(() => UPLOAD_FAILED)),
        ),
      ),
    );
  }

  private load(eventId: number): void {
    this.loading.set(true);
    this.failed.set(false);
    this.crud
      .getFilteredTableRows(GALLERY_TABLE, 'sort_order', true, PAGE_SIZE, 0, [
        { column_name: 'event_id', column_value: String(eventId) },
      ])
      .pipe(
        map((result) =>
          dataResultsToRecords(result)
            .map((row): GalleryRow => ({
              id: Number(row['id']),
              sortOrder: Number(row['sort_order']) || 0,
              altText: row['alt_text'] ?? '',
              caption: row['caption'] ?? '',
              credit: row['credit'] ?? '',
            }))
            // In number order, whatever order the read sorted the text in.
            .sort((a, b) => a.sortOrder - b.sortOrder || a.id - b.id),
        ),
        takeUntilDestroyed(this.destroyRef),
      )
      .subscribe({
        next: (rows) => {
          this.rows.set(rows);
          this.loading.set(false);
        },
        error: () => {
          this.failed.set(true);
          this.loading.set(false);
        },
      });
  }
}
