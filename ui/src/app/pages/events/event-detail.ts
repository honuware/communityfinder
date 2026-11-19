import { HttpErrorResponse } from '@angular/common/http';
import { Component, computed, DestroyRef, inject, OnInit, signal } from '@angular/core';
import { takeUntilDestroyed } from '@angular/core/rxjs-interop';
import { ActivatedRoute, RouterLink } from '@angular/router';
import { catchError, map, of, switchMap, tap } from 'rxjs';
import { MatButtonModule } from '@angular/material/button';
import { MatIconModule } from '@angular/material/icon';
import { MatProgressSpinnerModule } from '@angular/material/progress-spinner';
import { HONUWARE_API_BASE } from '@honuware/ui/access';
import { AuthService, hasPermission } from '@honuware/ui/auth';
import { COMMUNITY_ACCESS } from '../../core/community-access/community-access';
import { EventPhotos, FULL_VIEW_SIZE, HERO_SIZE } from '../../core/services/event-photos.service';
import {
  EMPTY_VOCABULARIES,
  EventVocabularies,
  EventVocabulariesService,
  vocabularyLabel,
} from '../../core/services/event-vocabularies.service';
import { EventDetail, isPublicEvent } from '../../core/types/events.types';
import {
  absoluteUrl,
  eventHasEnded,
  eventIcsUrl,
  eventPagePath,
  googleCalendarUrl,
  seriesPagePath,
} from '../../core/utils/event-links';
import {
  formatEventDateLine,
  formatEventTimeRange,
  nowUs,
} from '../../core/utils/events-date.utils';
import { JSON_LD_GALLERY_IMAGES, PageMeta } from '../../core/utils/page-meta';
import { PhotoGallery } from '../../shared/components/photo-gallery/photo-gallery';
import { VideoEmbed } from '../../shared/components/video-embed/video-embed';
import { tagChips } from './events-view';

// /events/:id — one event in full: its photo, when, where, cost, Add to
// calendar (the ICS download plus Google Calendar's quick-add, OQ1), the
// description, its own link, its series, its chips, then its video and its
// gallery (CM3 §5.5). Sets the page's full metadata and its schema.org
// JSON-LD. The photo, video and gallery are the server's picks: its own, else
// its series' (and for the photo, else its venue's), so the page never asks
// whether one exists. A signed-in curator can also open an event that isn't
// approved yet: it shows a preview banner, and no Add to calendar, since the
// download serves public events only (OQ6 (c)). An event that's over says so
// and drops Add to calendar too (EV4 §3.3).
@Component({
  selector: 'app-event-detail',
  imports: [
    RouterLink,
    MatButtonModule,
    MatIconModule,
    MatProgressSpinnerModule,
    PhotoGallery,
    VideoEmbed,
  ],
  templateUrl: './event-detail.html',
  styleUrl: './event-detail.scss',
})
export class EventDetailPage implements OnInit {
  private access = inject(COMMUNITY_ACCESS);
  private photos = inject(EventPhotos);
  private apiBase = inject(HONUWARE_API_BASE);
  private auth = inject(AuthService);
  private vocabularyService = inject(EventVocabulariesService);
  private route = inject(ActivatedRoute);
  private pageMeta = inject(PageMeta);
  private destroyRef = inject(DestroyRef);

  readonly loading = signal(true);
  readonly notFound = signal(false);
  readonly failed = signal(false);
  readonly event = signal<EventDetail | null>(null);
  readonly vocabularies = signal<EventVocabularies>(EMPTY_VOCABULARIES);
  readonly canManage = signal(false);

  // The hero, as the page shows it; '' for none.
  readonly photoUrl = computed(() => {
    const event = this.event();
    return event ? this.photos.photoFor(event, HERO_SIZE) : '';
  });
  readonly pageUrl = computed(() => {
    const event = this.event();
    return event ? absoluteUrl(this.pageMeta.origin, eventPagePath(event.id)) : '';
  });

  readonly isPublic = computed(() => {
    const event = this.event();
    return event ? isPublicEvent(event) : true;
  });
  // EV4 §3.3: an old event's page still loads, so say it's over and stop
  // offering it for a calendar. Read when the event arrives.
  readonly hasPassed = computed(() => {
    const event = this.event();
    return event ? eventHasEnded(event, nowUs()) : false;
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
    this.loading.set(true);
    this.notFound.set(false);
    this.failed.set(false);
    this.event.set(null);
  }

  // The page and its metadata at once: the payload says which photo it shows,
  // so og:image never points at a missing one. The gallery's first few join
  // the JSON-LD at full size.
  private show(event: EventDetail): void {
    this.event.set(event);
    const origin = this.pageMeta.origin;
    const hero = this.photoUrl();
    this.pageMeta.setEventPage(
      event,
      hero ? absoluteUrl(origin, hero) : '',
      event.gallery
        .slice(0, JSON_LD_GALLERY_IMAGES)
        .map((photo) => absoluteUrl(origin, this.photos.galleryPhotoUrl(photo, FULL_VIEW_SIZE))),
    );
  }
}
