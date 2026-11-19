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
import { SiteConfigService } from '../../../core/services/site-config.service';
import { GalleryPhoto } from '../../../core/types/events.types';

// How far a finger has to travel sideways to count as a swipe, in pixels.
const SWIPE_DISTANCE = 40;

// CM3 §5.3: an event's or a series' gallery. A grid of thumbnails, each a
// button that opens the photo full size in a dialog: arrow keys and a swipe
// move through them (wrapping), Escape or the close button or the backdrop
// closes it, and focus goes back to the thumbnail it came from. Each photo
// carries its alt text, and its caption and credit under it. "Ask us to
// remove a photo" writes to the site's contact address (OQ8), naming the
// page and, from the viewer, the photo.
@Component({
  selector: 'app-photo-gallery',
  imports: [A11yModule, MatIconModule],
  templateUrl: './photo-gallery.html',
  styleUrl: './photo-gallery.scss',
})
export class PhotoGallery {
  private photoUrls = inject(EventPhotos);
  private siteConfig = inject(SiteConfigService);
  private injector = inject(Injector);

  readonly photos = input.required<GalleryPhoto[]>();
  // What the gallery is of ("Leather Night"), for the removal request.
  readonly title = input.required<string>();
  // The page's absolute address, for the removal request.
  readonly pageUrl = input('');

  // The open photo's place in the list; null while the viewer is closed. A
  // new list (the page moved on) closes it.
  readonly openIndex = linkedSignal<GalleryPhoto[], number | null>({
    source: this.photos,
    computation: () => null,
  });

  private opener: HTMLElement | null = null;
  private touchStart: { x: number; y: number } | null = null;
  private readonly closeButton = viewChild<ElementRef<HTMLButtonElement>>('closeButton');

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
  readonly galleryRemovalHref = computed(() => this.removalHref(''));
  readonly photoRemovalHref = computed(() => {
    const photo = this.current();
    return photo ? this.removalHref(this.describe(photo)) : '';
  });

  open(index: number, opener: EventTarget | null): void {
    this.opener = opener instanceof HTMLElement ? opener : null;
    this.openIndex.set(index);
    afterNextRender(() => this.closeButton()?.nativeElement.focus(), { injector: this.injector });
  }

  close(): void {
    if (this.openIndex() === null) {
      return;
    }
    this.openIndex.set(null);
    this.opener?.focus();
    this.opener = null;
  }

  next(): void {
    this.step(1);
  }

  previous(): void {
    this.step(-1);
  }

  // The viewer's keys, wherever focus is while it's open.
  @HostListener('document:keydown', ['$event'])
  onKeydown(event: KeyboardEvent): void {
    if (this.openIndex() === null) {
      return;
    }
    if (event.key === 'Escape') {
      event.preventDefault();
      this.close();
    } else if (event.key === 'ArrowRight') {
      event.preventDefault();
      this.next();
    } else if (event.key === 'ArrowLeft') {
      event.preventDefault();
      this.previous();
    }
  }

  // A press on the dimmed backdrop itself, not on anything in it, closes.
  onBackdropClick(event: MouseEvent): void {
    if (event.target === event.currentTarget) {
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

  private step(by: number): void {
    const index = this.openIndex();
    const count = this.photos().length;
    if (index === null || count < 2) {
      return;
    }
    this.openIndex.set((index + by + count) % count);
  }

  // "Photo 2 of 4 ("Midnight on the floor.")", for the request's body.
  private describe(photo: GalleryPhoto): string {
    const index = this.photos().indexOf(photo);
    const label = `photo ${index + 1} of ${this.photos().length}`;
    return photo.caption ? `${label} ("${photo.caption}")` : label;
  }

  private removalHref(which: string): string {
    const subject = `Photo removal: ${this.title()}`;
    const page = this.pageUrl() ? ` on ${this.pageUrl()}` : '';
    const body = which
      ? `Please remove ${which}${page}.\n\n`
      : `Please remove a photo${page}.\n\nWhich one (its number or caption):\n\n`;
    return (
      `mailto:${this.siteConfig.config.contactEmail}` +
      `?subject=${encodeURIComponent(subject)}&body=${encodeURIComponent(body)}`
    );
  }
}
