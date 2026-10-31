import { Component, computed, inject } from '@angular/core';
import { toSignal } from '@angular/core/rxjs-interop';
import { MatButtonModule } from '@angular/material/button';
import { MatIconModule } from '@angular/material/icon';
import { MatProgressSpinnerModule } from '@angular/material/progress-spinner';
import { RouterLink } from '@angular/router';
import { catchError, of, switchMap } from 'rxjs';
import { GUIDE_ACCESS } from '../../core/guide-access/guide-access';
import { guideVocabularyLabel } from '../../core/services/guide-vocabularies.service';
import { GuidePage } from '../../core/types/guide.types';
import { PLACE_KIND_LABELS } from '../../core/utils/guide-labels';
import { AdultGate } from '../../shared/components/adult-gate/adult-gate';
import { FreshnessFooter } from '../../shared/components/freshness-footer/freshness-footer';
import { LinkOutCard } from '../../shared/components/link-out-card/link-out-card';
import { SceneChips } from '../../shared/components/scene-chips/scene-chips';
import { guideVocabularies, loadEach, routeParam, titleFromLoad } from './guide-page-state';

// /guide/places/:slug — a place: a park, a beach, a landmark. The structured
// facts live here; the writing about it is a guide page (GD1 §4), linked when
// there's a published page with the place's own slug, the same convention as
// scene intros and neighborhood rundowns. An 18+ place sits behind the gate.
@Component({
  selector: 'app-place-page',
  imports: [
    RouterLink,
    MatButtonModule,
    MatIconModule,
    MatProgressSpinnerModule,
    AdultGate,
    FreshnessFooter,
    LinkOutCard,
    SceneChips,
  ],
  templateUrl: './place-page.html',
})
export class PlacePage {
  private access = inject(GUIDE_ACCESS);
  private readonly slug$ = routeParam('slug');

  readonly load = loadEach(this.slug$, (slug) => this.access.getPlace(slug));
  readonly vocabularies = guideVocabularies();
  // The place's own article, if one's published; nothing at all if not.
  readonly article = toSignal(
    this.slug$.pipe(
      switchMap((slug) =>
        this.access.getGuidePage(slug).pipe(catchError(() => of<GuidePage | null>(null))),
      ),
    ),
    { initialValue: null },
  );

  readonly place = computed(() => this.load().data?.place ?? null);
  // "Park & beach · Capitol Hill"
  readonly lead = computed(() => {
    const place = this.place();
    if (!place) {
      return '';
    }
    const neighborhood = place.neighborhood_slug
      ? guideVocabularyLabel(this.vocabularies().neighborhoods, place.neighborhood_slug)
      : '';
    return [PLACE_KIND_LABELS[place.place_kind] ?? '', neighborhood]
      .filter((part) => part !== '')
      .join(' · ');
  });

  constructor() {
    titleFromLoad(this.load, (detail) => detail.place.name, 'Place not found');
  }
}
