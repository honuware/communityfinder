import { inject, Injectable } from '@angular/core';
import { PhotoUrlBuilder } from '@honuware/ui/access';
import { GalleryPhoto, PhotoRef } from '../types/events.types';

// The sizes CM3 asks honuware's scaler for. Each is a bounding box: the photo
// is scaled to fit it, and CF's public cap (1600×1200, CM3 §1.5) lets every
// one of them through whole.
export interface PhotoSize {
  width: number;
  height: number;
}

// An event row's thumbnail (EV2).
export const LIST_THUMB_SIZE: PhotoSize = { width: 160, height: 160 };
// The event and series pages' hero, and og:image (EV2).
export const HERO_SIZE: PhotoSize = { width: 1200, height: 630 };
// A gallery's grid.
export const GALLERY_THUMB_SIZE: PhotoSize = { width: 480, height: 360 };
// The gallery's full view, and the JSON-LD's gallery images.
export const FULL_VIEW_SIZE: PhotoSize = { width: 1600, height: 1200 };

// CM3 §5.2: the one place event and series photo addresses are built. The
// server says which photo a row shows (`photo`: its own, else its series',
// else its venue's), so pages never guess, ask has_photo, or hide a failed
// image.
@Injectable({ providedIn: 'root' })
export class EventPhotos {
  private urls = inject(PhotoUrlBuilder);

  // The address of what `owner` shows, at `size`; '' when it shows none.
  photoFor(owner: { photo: PhotoRef | null }, size: PhotoSize): string {
    const photo = owner.photo;
    return photo ? this.urls.scaledPhotoUrl(photo.table, photo.id, size.width, size.height) : '';
  }

  // A gallery picture's address: its photo is its `event_gallery_photos` row's.
  galleryPhotoUrl(photo: Pick<GalleryPhoto, 'id'>, size: PhotoSize): string {
    return this.urls.scaledPhotoUrl('event_gallery_photos', photo.id, size.width, size.height);
  }
}
