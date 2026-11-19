import { Injectable } from '@angular/core';
import { GallerySettings } from '../events-admin-access/events-admin-access';
import { SubmissionProblem } from '../event-submissions-access/event-submissions-access';

// The server's range for the gallery cap (CM3 OQ5, gallery_settings.h), the
// mock's own copy. Pages never read it: they take it from the settings GET.
const MAX_PHOTOS = { default: 12, min: 1, max: 60 };

// The offline stand-in for event_gallery_max_photos in config_secrets. One
// instance for the whole app, so a cap saved on the settings page applies to
// the offline event editor's next "Add photo".
@Injectable({ providedIn: 'root' })
export class GallerySettingsMockStore {
  private value = MAX_PHOTOS.default;

  get maxPhotos(): number {
    return this.value;
  }

  settings(): GallerySettings {
    return { event_gallery_max_photos: { value: this.value, ...MAX_PHOTOS } };
  }

  // The server's save: an out-of-range cap saves nothing.
  save(maxPhotos: number): SubmissionProblem | null {
    if (!Number.isInteger(maxPhotos) || maxPhotos < MAX_PHOTOS.min || maxPhotos > MAX_PHOTOS.max) {
      return {
        status: 400,
        error_code: 'MAX_PHOTOS_OUT_OF_RANGE',
        field: 'event_gallery_max_photos',
        date_index: null,
        detail: 'event_gallery_max_photos is outside its allowed range',
      };
    }
    this.value = maxPhotos;
    return null;
  }
}
