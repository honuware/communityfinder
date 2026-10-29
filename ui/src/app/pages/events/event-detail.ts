import { HttpErrorResponse } from '@angular/common/http';
import { Component, computed, DestroyRef, inject, OnInit, signal } from '@angular/core';
import { takeUntilDestroyed } from '@angular/core/rxjs-interop';
import { ActivatedRoute, RouterLink } from '@angular/router';
import { catchError, map, of, Subscription, switchMap, tap } from 'rxjs';
import { MatButtonModule } from '@angular/material/button';
import { MatIconModule } from '@angular/material/icon';
import { MatProgressSpinnerModule } from '@angular/material/progress-spinner';
import { HONUWARE_API_BASE, HONUWARE_PHOTO_ACCESS, PhotoUrlBuilder } from '@honuware/ui/access';
import { AuthService, hasPermission } from '@honuware/ui/auth';
import { COMMUNITY_ACCESS } from '../../core/community-access/community-access';
import {
  EMPTY_VOCABULARIES,
  EventVocabularies,
  EventVocabulariesService,
  vocabularyLabel,
} from '../../core/services/event-vocabularies.service';
import { PublicEvent } from '../../core/types/events.types';
import {
  absoluteUrl,
  eventIcsUrl,
  eventPagePath,
  googleCalendarUrl,
  seriesPagePath,
} from '../../core/utils/event-links';
import { formatEventDateLine, formatEventTimeRange } from '../../core/utils/events-date.utils';
import { PageMeta } from '../../core/utils/page-meta';
import { tagChips } from './events-view';

// The photo as the detail page shows it, and as og:image / the JSON-LD use it.
const PHOTO_WIDTH = 1200;
const PHOTO_HEIGHT = 630;

// /events/:id — one event in full: when, where, cost, description, its own
// link, its series, its chips and photo, and Add to calendar (the ICS
// download plus Google Calendar's quick-add, OQ1). Sets the page's full
// metadata and its schema.org JSON-LD. A signed-in curator can also open an
// event that isn't approved yet: it shows a preview banner, and no Add to
// calendar, since the download serves approved events only (OQ6 (c)).
@Component({
  selector: 'app-event-detail',
  imports: [RouterLink, MatButtonModule, MatIconModule, MatProgressSpinnerModule],
  templateUrl: './event-detail.html',
  styleUrl: './event-detail.scss',
})
export class EventDetailPage implements OnInit {
  private access = inject(COMMUNITY_ACCESS);
  private photoAccess = inject(HONUWARE_PHOTO_ACCESS);
  private photoUrls = inject(PhotoUrlBuilder);
  private apiBase = inject(HONUWARE_API_BASE);
  private auth = inject(AuthService);
  private vocabularyService = inject(EventVocabulariesService);
  private route = inject(ActivatedRoute);
  private pageMeta = inject(PageMeta);
  private destroyRef = inject(DestroyRef);
  private photoCheck?: Subscription;

  readonly loading = signal(true);
  readonly notFound = signal(false);
  readonly failed = signal(false);
  readonly event = signal<PublicEvent | null>(null);
  readonly photoUrl = signal('');
  readonly vocabularies = signal<EventVocabularies>(EMPTY_VOCABULARIES);
  readonly canManage = signal(false);

  readonly isPublic = computed(() => {
    const status = this.event()?.status;
    return status === undefined || status === 'approved';
  });
  readonly dateLine = computed(() => {
    const event = this.event();
    return event ? formatEventDateLine(event.starts_at_us) : '';
  });
  readonly timeRange = computed(() => {
    const event = this.event();
    return event ? formatEventTimeRange(event.starts_at_us, event.ends_at_us) : '';
  });
  readonly icsUrl = computed(() => {
    const event = this.event();
    return event ? eventIcsUrl(this.apiBase, event.id) : '';
  });
  readonly googleUrl = computed(() => {
    const event = this.event();
    return event
      ? googleCalendarUrl(event, absoluteUrl(this.pageMeta.origin, eventPagePath(event.id)))
      : '';
  });
  readonly seriesPath = computed(() => {
    const slug = this.event()?.series_slug;
    return slug ? seriesPagePath(slug) : '';
  });
  readonly neighborhood = computed(() => {
    const slug = this.event()?.neighborhood_slug;
    return slug ? { slug, label: vocabularyLabel(this.vocabularies().neighborhoods, slug) } : null;
  });
  readonly chips = computed(() => {
    const event = this.event();
    return event ? tagChips(event, this.vocabularies()) : [];
  });

  ngOnInit(): void {
    this.auth.authData$
      .pipe(takeUntilDestroyed(this.destroyRef))
      .subscribe((authData) => this.canManage.set(hasPermission(authData, 'manage_events')));
    this.vocabularyService.vocabularies$
      .pipe(takeUntilDestroyed(this.destroyRef))
      .subscribe((vocabularies) => this.vocabularies.set(vocabularies));
    // The component stays put from one event to the next (/events/1 →
    // /events/2), so each id change loads afresh.
    this.route.paramMap
      .pipe(
        map((params) => Number(params.get('id'))),
        tap(() => this.startLoading()),
        switchMap((id) =>
          Number.isInteger(id) && id > 0
            ? this.access.getEvent(id).pipe(
                map((event) => ({ event, status: 200 })),
                catchError((error: HttpErrorResponse) => of({ event: null, status: error.status })),
              )
            : of({ event: null, status: 404 }),
        ),
        takeUntilDestroyed(this.destroyRef),
      )
      .subscribe(({ event, status }) => {
        this.loading.set(false);
        if (event) {
          this.show(event);
        } else if (status === 404) {
          this.notFound.set(true);
          this.pageMeta.set({
            title: 'Event not found',
            description: 'This event is not listed, or is no longer listed.',
            canonicalPath: '/events',
          });
        } else {
          this.failed.set(true);
        }
      });
  }

  private startLoading(): void {
    this.photoCheck?.unsubscribe();
    this.loading.set(true);
    this.notFound.set(false);
    this.failed.set(false);
    this.event.set(null);
    this.photoUrl.set('');
  }

  // The page and its metadata; the photo joins both once the check says
  // there is one (og:image must never point at a missing photo).
  private show(event: PublicEvent): void {
    this.event.set(event);
    this.pageMeta.setEventPage(event);
    this.photoCheck = this.photoAccess
      .hasPhoto('events', event.id)
      .pipe(
        catchError(() => of({ has_photo: false })),
        takeUntilDestroyed(this.destroyRef),
      )
      .subscribe(({ has_photo }) => {
        if (!has_photo) {
          return;
        }
        const photoUrl = absoluteUrl(
          this.pageMeta.origin,
          this.photoUrls.scaledPhotoUrl('events', event.id, PHOTO_WIDTH, PHOTO_HEIGHT),
        );
        this.photoUrl.set(photoUrl);
        this.pageMeta.setEventPage(event, photoUrl);
      });
  }
}
