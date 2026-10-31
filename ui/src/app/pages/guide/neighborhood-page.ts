import { Component, computed, inject } from '@angular/core';
import { MatButtonModule } from '@angular/material/button';
import { MatIconModule } from '@angular/material/icon';
import { MatProgressSpinnerModule } from '@angular/material/progress-spinner';
import { RouterLink } from '@angular/router';
import { GUIDE_ACCESS } from '../../core/guide-access/guide-access';
import { NEIGHBORHOOD_REGIONS } from '../../core/utils/guide-labels';
import {
  EntityCard,
  organizationCardItem,
  placeCardItem,
  venueCardItem,
} from '../../shared/components/entity-card/entity-card';
import { FreshnessFooter } from '../../shared/components/freshness-footer/freshness-footer';
import { GuideMarkdown } from './guide-markdown';
import { guideVocabularies, loadEach, routeParam, titleFromLoad } from './guide-page-state';

// /guide/neighborhoods/:slug — GD1 §7's neighborhood page: its rundown (the
// published new-to-seattle page with the neighborhood's slug, which carries
// the dated rent and transit lines), then the venues, organizations and
// places here. The footer is the rundown's: the neighborhood itself is a
// vocabulary entry, with nothing to verify.
@Component({
  selector: 'app-neighborhood-page',
  imports: [
    RouterLink,
    MatButtonModule,
    MatIconModule,
    MatProgressSpinnerModule,
    EntityCard,
    FreshnessFooter,
    GuideMarkdown,
  ],
  templateUrl: './neighborhood-page.html',
})
export class NeighborhoodPage {
  private access = inject(GUIDE_ACCESS);

  readonly load = loadEach(routeParam('slug'), (slug) => this.access.getNeighborhood(slug));
  readonly vocabularies = guideVocabularies();

  readonly region = computed(() => {
    const region = this.load().data?.neighborhood.region;
    return NEIGHBORHOOD_REGIONS.find((entry) => entry.region === region)?.label ?? '';
  });
  readonly venueCards = computed(() => (this.load().data?.venues ?? []).map(venueCardItem));
  readonly organizationCards = computed(() =>
    (this.load().data?.organizations ?? []).map(organizationCardItem),
  );
  readonly placeCards = computed(() => (this.load().data?.places ?? []).map(placeCardItem));
  readonly listed = computed(
    () => this.venueCards().length + this.organizationCards().length + this.placeCards().length,
  );

  constructor() {
    titleFromLoad(
      this.load,
      (detail) => detail.neighborhood.display_name,
      'Neighborhood not found',
    );
  }
}
