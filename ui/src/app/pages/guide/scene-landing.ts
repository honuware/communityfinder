import { Component, computed, inject } from '@angular/core';
import { MatButtonModule } from '@angular/material/button';
import { MatIconModule } from '@angular/material/icon';
import { MatProgressSpinnerModule } from '@angular/material/progress-spinner';
import { RouterLink } from '@angular/router';
import { GUIDE_ACCESS } from '../../core/guide-access/guide-access';
import {
  EntityCard,
  EntityCardItem,
  organizationCardItem,
  placeCardItem,
  venueCardItem,
} from '../../shared/components/entity-card/entity-card';
import { FreshnessFooter } from '../../shared/components/freshness-footer/freshness-footer';
import { LinkOutCard } from '../../shared/components/link-out-card/link-out-card';
import { GuideMarkdown } from './guide-markdown';
import { guideVocabularies, loadEach, routeParam, titleFromLoad } from './guide-page-state';
import { GuideUpcomingEvents } from './guide-upcoming-events';

// /guide/scenes/:slug — GD1 §7's scene landing. A scene we cover ourselves
// (`full`): its intro, then its venues, organizations and places, what's
// coming up, and where it talks. A `link_out` scene: its intro and its own
// curators' links, credited, plus the shared spaces we'd list anyway (GD1 §2,
// §6.6), and no claim to cover the rest.
@Component({
  selector: 'app-scene-landing',
  imports: [
    RouterLink,
    MatButtonModule,
    MatIconModule,
    MatProgressSpinnerModule,
    EntityCard,
    FreshnessFooter,
    GuideMarkdown,
    GuideUpcomingEvents,
    LinkOutCard,
  ],
  templateUrl: './scene-landing.html',
})
export class SceneLandingPage {
  private access = inject(GUIDE_ACCESS);

  readonly load = loadEach(routeParam('slug'), (slug) => this.access.getScene(slug));
  readonly vocabularies = guideVocabularies();

  readonly linkOut = computed(() => this.load().data?.scene.coverage === 'link_out');
  readonly venueCards = computed(() => (this.load().data?.venues ?? []).map(venueCardItem));
  readonly organizationCards = computed(() =>
    (this.load().data?.organizations ?? []).map(organizationCardItem),
  );
  readonly placeCards = computed(() => (this.load().data?.places ?? []).map(placeCardItem));
  // Everything listed here, for a link-out scene's "shared spaces".
  readonly allCards = computed<EntityCardItem[]>(() => [
    ...this.venueCards(),
    ...this.organizationCards(),
    ...this.placeCards(),
  ]);

  constructor() {
    titleFromLoad(this.load, (detail) => detail.scene.display_name, 'Scene not found');
  }
}
