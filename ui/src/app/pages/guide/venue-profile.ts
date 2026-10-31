import { Component, computed, inject, signal } from '@angular/core';
import { MatButtonModule } from '@angular/material/button';
import { MatIconModule } from '@angular/material/icon';
import { MatProgressSpinnerModule } from '@angular/material/progress-spinner';
import { RouterLink } from '@angular/router';
import { PhotoUrlBuilder } from '@honuware/ui/access';
import { GUIDE_ACCESS } from '../../core/guide-access/guide-access';
import { guideVocabularyLabel } from '../../core/services/guide-vocabularies.service';
import { AdultGate } from '../../shared/components/adult-gate/adult-gate';
import { FreshnessFooter } from '../../shared/components/freshness-footer/freshness-footer';
import { LinkOutCard } from '../../shared/components/link-out-card/link-out-card';
import { SceneChips } from '../../shared/components/scene-chips/scene-chips';
import { guideVocabularies, loadEach, routeParam, titleFromLoad } from './guide-page-state';
import { GuideUpcomingEvents } from './guide-upcoming-events';

// /guide/venues/:slug — GD1 §7's venue profile: name, neighborhood and status;
// its photo; the crowd; the practical block (address, website, price); scene
// chips; where it talks; what's coming up there; and the freshness footer. A
// closed venue keeps the same page with a "Closed" banner and its closure
// note: that page is its graveyard entry. An 18+ venue sits behind the gate.
@Component({
  selector: 'app-venue-profile',
  imports: [
    RouterLink,
    MatButtonModule,
    MatIconModule,
    MatProgressSpinnerModule,
    AdultGate,
    FreshnessFooter,
    GuideUpcomingEvents,
    LinkOutCard,
    SceneChips,
  ],
  templateUrl: './venue-profile.html',
})
export class VenueProfilePage {
  private access = inject(GUIDE_ACCESS);
  private photoUrls = inject(PhotoUrlBuilder);

  readonly load = loadEach(routeParam('slug'), (slug) => this.access.getVenue(slug));
  readonly vocabularies = guideVocabularies();

  readonly venue = computed(() => this.load().data?.venue ?? null);
  readonly retired = computed(() => {
    const status = this.venue()?.status;
    return status === 'closed' || status === 'moved';
  });
  readonly neighborhood = computed(() => {
    const slug = this.venue()?.neighborhood_slug;
    return slug ? guideVocabularyLabel(this.vocabularies().neighborhoods, slug) : '';
  });

  // Most venues have no photo yet: the request 404s and the image is hidden.
  readonly photoUrl = computed(() => {
    const venue = this.venue();
    return venue ? this.photoUrls.scaledPhotoUrl('venues', venue.venue_id, 1200, 675) : '';
  });
  private readonly failedPhotoUrl = signal('');
  readonly showPhoto = computed(
    () => !!this.photoUrl() && this.photoUrl() !== this.failedPhotoUrl(),
  );

  constructor() {
    titleFromLoad(this.load, (detail) => detail.venue.name, 'Venue not found');
  }

  photoFailed(): void {
    this.failedPhotoUrl.set(this.photoUrl());
  }
}
