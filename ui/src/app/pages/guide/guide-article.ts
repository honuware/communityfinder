import { Component, computed, inject } from '@angular/core';
import { toObservable, toSignal } from '@angular/core/rxjs-interop';
import { MatButtonModule } from '@angular/material/button';
import { MatIconModule } from '@angular/material/icon';
import { MatProgressSpinnerModule } from '@angular/material/progress-spinner';
import { RouterLink } from '@angular/router';
import { catchError, forkJoin, map, Observable, of, switchMap } from 'rxjs';
import { GUIDE_ACCESS } from '../../core/guide-access/guide-access';
import { ORGANIZATION_KIND_LABELS, SECTION_LABELS } from '../../core/utils/guide-labels';
import { AdultGate } from '../../shared/components/adult-gate/adult-gate';
import { FreshnessFooter } from '../../shared/components/freshness-footer/freshness-footer';
import { GuideMarkdown } from './guide-markdown';
import { loadEach, routeParam, titleFromLoad } from './guide-page-state';

// A listing that shares a scene tag with the article.
export interface RelatedListing {
  name: string;
  path: string;
  // "Venue", "Sports league", "Park & beach".
  kind: string;
  adult: boolean;
}

const RELATED_LIMIT = 10;

function sameSlugs(first: string[], second: string[]): boolean {
  return first.length === second.length && first.every((slug, index) => slug === second[index]);
}

// /guide/pages/:slug — GD1 §7's guide article: title, the markdown body (its
// links to other guide pages navigate in-app), the listings that share its
// scene tags, and the freshness footer. An 18+ page sits behind the gate.
// Unknown and unpublished slugs are "not found"; a manage_guide curator gets
// unpublished pages too, marked as a preview (GD2 §4.9).
@Component({
  selector: 'app-guide-article',
  imports: [
    RouterLink,
    MatButtonModule,
    MatIconModule,
    MatProgressSpinnerModule,
    AdultGate,
    FreshnessFooter,
    GuideMarkdown,
  ],
  templateUrl: './guide-article.html',
})
export class GuideArticlePage {
  private access = inject(GUIDE_ACCESS);

  readonly load = loadEach(routeParam('slug'), (slug) => this.access.getGuidePage(slug));

  readonly sectionLabel = computed(() => {
    const section = this.load().data?.section;
    return section ? (SECTION_LABELS[section] ?? '') : '';
  });

  private readonly sceneTags = computed(() => this.load().data?.scene_tag_slugs ?? [], {
    equal: sameSlugs,
  });
  // A failed read just leaves the list off.
  readonly related = toSignal(
    toObservable(this.sceneTags).pipe(switchMap((tags) => this.relatedListings(tags))),
    { initialValue: [] },
  );

  constructor() {
    titleFromLoad(this.load, (page) => page.title, 'Page not found');
  }

  private relatedListings(sceneTags: string[]): Observable<RelatedListing[]> {
    if (!sceneTags.length) {
      return of([]);
    }
    return forkJoin([
      forkJoin(sceneTags.map((scene) => this.access.getVenues({ scene }))),
      forkJoin(sceneTags.map((scene) => this.access.getOrganizations({ scene }))),
      this.access.getPlaces(),
    ]).pipe(
      map(([venueLists, organizationLists, places]) => {
        const listings: RelatedListing[] = [
          ...venueLists.flat().map((venue) => ({
            name: venue.name,
            path: `/guide/venues/${venue.slug}`,
            kind: 'Venue',
            adult: venue.adult,
          })),
          ...organizationLists.flat().map((organization) => ({
            name: organization.name,
            path: `/guide/orgs/${organization.slug}`,
            kind: ORGANIZATION_KIND_LABELS[organization.org_kind] ?? 'Organization',
            adult: organization.adult,
          })),
          ...places
            .filter((place) => place.scene_tag_slugs.some((tag) => sceneTags.includes(tag)))
            .map((place) => ({
              name: place.name,
              path: `/guide/places/${place.slug}`,
              kind: 'Place',
              adult: place.adult,
            })),
        ];
        // A listing with two of the article's tags comes back twice.
        const unique = listings.filter(
          (listing, index) => listings.findIndex((other) => other.path === listing.path) === index,
        );
        return unique.slice(0, RELATED_LIMIT);
      }),
      catchError(() => of([])),
    );
  }
}
