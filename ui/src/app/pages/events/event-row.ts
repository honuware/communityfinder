import { Component, computed, inject, input, signal } from '@angular/core';
import { RouterLink } from '@angular/router';
import { PhotoUrlBuilder } from '@honuware/ui/access';
import { PublicEvent } from '../../core/types/events.types';
import {
  EMPTY_VOCABULARIES,
  EventVocabularies,
  vocabularyLabel,
} from '../../core/services/event-vocabularies.service';
import { eventPagePath } from '../../core/utils/event-links';
import { formatEventShortDate, formatEventTimeRange } from '../../core/utils/events-date.utils';
import { tagChips } from './events-view';

// One event in a list: photo thumb, time, title, place, neighborhood, and
// its category / scene chips (each a link to that filtered list). The list
// page groups rows under day headers; the series page passes `showDate`.
@Component({
  selector: 'app-event-row',
  imports: [RouterLink],
  templateUrl: './event-row.html',
  styleUrl: './event-row.scss',
})
export class EventRow {
  private photoUrls = inject(PhotoUrlBuilder);

  readonly event = input.required<PublicEvent>();
  readonly vocabularies = input<EventVocabularies>(EMPTY_VOCABULARIES);
  // Name the day too, where no day header sits above the row.
  readonly showDate = input(false);

  // Most events have no photo; the thumb's request 404s and it's hidden.
  readonly photoFailed = signal(false);

  readonly pagePath = computed(() => eventPagePath(this.event().id));
  readonly photoUrl = computed(() =>
    this.photoUrls.scaledPhotoUrl('events', this.event().id, 160, 160),
  );
  readonly when = computed(() => {
    const event = this.event();
    const times = formatEventTimeRange(event.starts_at_us, event.ends_at_us);
    return this.showDate() ? `${formatEventShortDate(event.starts_at_us)} · ${times}` : times;
  });
  readonly place = computed(() => this.event().venue_name || this.event().location_text);
  readonly neighborhood = computed(() => {
    const slug = this.event().neighborhood_slug;
    return slug ? { slug, label: vocabularyLabel(this.vocabularies().neighborhoods, slug) } : null;
  });
  readonly chips = computed(() => tagChips(this.event(), this.vocabularies()));
}
