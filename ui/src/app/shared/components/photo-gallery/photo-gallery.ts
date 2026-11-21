import {
  afterNextRender,
  Component,
  computed,
  ElementRef,
  HostListener,
  inject,
  Injector,
  input,
  linkedSignal,
  viewChild,
} from '@angular/core';
import { A11yModule } from '@angular/cdk/a11y';
import { MatIconModule } from '@angular/material/icon';
import {
  EventPhotos,
  FULL_VIEW_SIZE,
  GALLERY_THUMB_SIZE,
} from '../../../core/services/event-photos.service';
import { GalleryPhoto } from '../../../core/types/events.types';
import { PhotoRemovalForm } from '../photo-removal-form/photo-removal-form';

// How far a finger has to travel sideways to count as a swipe, in pixels.
const SWIPE_DISTANCE = 40;

// Where the removal form is open: under the grid (no photo named), or in the
// viewer (the open photo).
export type RemovalFormPlace = 'gallery' | 'photo';

// CM3 §5.3: an event's or a series' gallery. A grid of thumbnails, each a
// button that opens the photo full size in a dialog: arrow keys and a swipe
// move through them (wrapping), Escape or the close button or the backdrop
// closes it, and focus goes back to the thumbnail it came from. Each photo
// carries its alt text, and its caption and credit under it.
//
// "Ask us to remove a photo" (under the grid) and "Ask us to remove this
// photo" (in the viewer) open a short form (Phase 2 §12, PhotoRemovalForm)
// that names the page and, from the viewer, the photo. While it's open in
// the viewer the photo stays put: no stepping, a press on the backdrop
// doesn't close it, and Escape closes the form first.
@Component({
  selector: 'app-photo-gallery',
  imports: [A11yModule, MatIconModule, PhotoRemovalForm],
  templateUrl: './photo-gallery.html',
  styleUrl: './photo-gallery.scss',
})
export class PhotoGallery {
  private photoUrls = inject(EventPhotos);
  private injector = inject(Injector);

  readonly photos = input.required<GalleryPhoto[]>();
  // What the gallery is of ("Leather Night"), for the removal request.
  readonly title = input.required<string>();
  // The page's absolute address, for the removal form's fallback email.
  readonly pageUrl = input('');

  // The open photo's place in the list; null while the viewer is closed. A
  // new list (the page moved on) closes it.
  readonly openIndex = linkedSignal<GalleryPhoto[], number | null>({
    source: this.photos,
    computation: () => null,
  });
  // A new list closes the removal form too.
  readonly removalForm = linkedSignal<GalleryPhoto[], RemovalFormPlace | null>({
    source: this.photos,
    computation: () => null,
  });

  private opener: HTMLElement | null = null;
  private touchStart: { x: number; y: number } | null = null;
  private readonly closeButton = viewChild<ElementRef<HTMLButtonElement>>('closeButton');
  // The two "Ask us…" buttons; each is drawn again when its form closes.
  private readonly galleryRemovalButton =
    viewChild<ElementRef<HTMLButtonElement>>('galleryRemovalButton');
  private readonly photoRemovalButton =
    viewChild<ElementRef<HTMLButtonElement>>('photoRemovalButton');

  readonly thumbs = computed(() =>
    this.photos().map((photo) => ({
      photo,
      url: this.photoUrls.galleryPhotoUrl(photo, GALLERY_THUMB_SIZE),
    })),
  );
  readonly current = computed(() => {
    const index = this.openIndex();
    return index === null ? null : (this.photos()[index] ?? null);
  });
  readonly currentUrl = computed(() => {
    const photo = this.current();
    return photo ? this.photoUrls.galleryPhotoUrl(photo, FULL_VIEW_SIZE) : '';
  });
  readonly position = computed(() => {
    const index = this.openIndex();
    return index === null ? '' : `${index + 1} of ${this.photos().length}`;
  });
  // 'photo 2 of 4 ("Midnight on the floor.")', for the viewer's request.
  readonly currentDescription = computed(() => {
    const photo = this.current();
    return photo ? this.describe(photo) : '';
  });

  open(index: number, opener: EventTarget | null): void {
    this.opener = opener instanceof HTMLElement ? opener : null;
    if (this.removalForm() === 'gallery') {
      this.removalForm.set(null);
    }
    this.openIndex.set(index);
    afterNextRender(() => this.closeButton()?.nativeElement.focus(), { injector: this.injector });
  }

  close(): void {
    if (this.openIndex() === null) {
      return;
    }
    if (this.removalForm() === 'photo') {
      this.removalForm.set(null);
    }
    this.openIndex.set(null);
    this.opener?.focus();
    this.opener = null;
  }

  openRemovalForm(place: RemovalFormPlace): void {
    this.removalForm.set(place);
  }

  // Focus goes back to the button that opened it, drawn again in its place.
  closeRemovalForm(): void {
    const place = this.removalForm();
    this.removalForm.set(null);
    afterNextRender(
      () =>
        (place === 'photo'
          ? this.photoRemovalButton()
          : this.galleryRemovalButton()
        )?.nativeElement.focus(),
      { injector: this.injector },
    );
  }

  next(): void {
    this.step(1);
  }

  previous(): void {
    this.step(-1);
  }

  // The viewer's keys, wherever focus is while it's open. Arrows typed in a
  // field move the caret, not the photo.
  @HostListener('document:keydown', ['$event'])
  onKeydown(event: KeyboardEvent): void {
    if (this.openIndex() === null) {
      return;
    }
    if (event.key === 'Escape') {
      event.preventDefault();
      if (this.removalForm() === 'photo') {
        this.closeRemovalForm();
      } else {
        this.close();
      }
    } else if (isTextField(event.target)) {
      return;
    } else if (event.key === 'ArrowRight') {
      event.preventDefault();
      this.next();
    } else if (event.key === 'ArrowLeft') {
      event.preventDefault();
      this.previous();
    }
  }

  // A press on the dimmed backdrop itself, not on anything in it, closes;
  // not while a removal request is being written, which it would throw away.
  onBackdropClick(event: MouseEvent): void {
    if (event.target === event.currentTarget && this.removalForm() !== 'photo') {
      this.close();
    }
  }

  onTouchStart(event: TouchEvent): void {
    const touch = event.changedTouches[0];
    this.touchStart = touch ? { x: touch.clientX, y: touch.clientY } : null;
  }

  // Sideways and far enough: a swipe left shows the next, right the previous.
  onTouchEnd(event: TouchEvent): void {
    const start = this.touchStart;
    const touch = event.changedTouches[0];
    this.touchStart = null;
    if (!start || !touch) {
      return;
    }
    const dx = touch.clientX - start.x;
    const dy = touch.clientY - start.y;
    if (Math.abs(dx) >= SWIPE_DISTANCE && Math.abs(dx) > Math.abs(dy)) {
      this.step(dx < 0 ? 1 : -1);
    }
  }

  // The request is about the photo on screen, so it stays on screen.
  private step(by: number): void {
    const index = this.openIndex();
    const count = this.photos().length;
    if (index === null || count < 2 || this.removalForm() === 'photo') {
      return;
    }
    this.openIndex.set((index + by + count) % count);
  }

  // 'photo 2 of 4 ("Midnight on the floor.")', for the request.
  private describe(photo: GalleryPhoto): string {
    const index = this.photos().indexOf(photo);
    const label = `photo ${index + 1} of ${this.photos().length}`;
    return photo.caption ? `${label} ("${photo.caption}")` : label;
  }
}

function isTextField(target: EventTarget | null): boolean {
  return (
    target instanceof HTMLTextAreaElement ||
    target instanceof HTMLSelectElement ||
    (target instanceof HTMLInputElement && target.type !== 'checkbox' && target.type !== 'radio') ||
    (target instanceof HTMLElement && target.isContentEditable)
  );
}
