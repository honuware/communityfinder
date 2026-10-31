import { Component, computed, inject } from '@angular/core';
import { MatButtonModule } from '@angular/material/button';
import { MatIconModule } from '@angular/material/icon';
import { MatProgressSpinnerModule } from '@angular/material/progress-spinner';
import { RouterLink } from '@angular/router';
import { of } from 'rxjs';
import { GUIDE_ACCESS } from '../../core/guide-access/guide-access';
import { GuideServiceKind } from '../../core/types/guide.types';
import { SERVICE_KIND_LABELS } from '../../core/utils/guide-labels';
import {
  EntityCard,
  EntityCardItem,
  serviceCardItem,
} from '../../shared/components/entity-card/entity-card';
import { GuidePageTitle, guideVocabularies, loadEach } from './guide-page-state';

// /guide/services — health care and everyday services, grouped by kind in
// GD1 §4's order (the health kinds first). Services have no pages of their
// own at MVP, so each card links out to the service's site and carries its
// phone number. A directory, not advice (GD1 §6.8): nothing here recommends.
@Component({
  selector: 'app-services-page',
  imports: [RouterLink, MatButtonModule, MatIconModule, MatProgressSpinnerModule, EntityCard],
  templateUrl: './services-page.html',
})
export class ServicesPage {
  private access = inject(GUIDE_ACCESS);

  readonly load = loadEach(of(null), () => this.access.getServices());
  readonly vocabularies = guideVocabularies();

  // Only the kinds that have something listed.
  readonly groups = computed(() => {
    const services = this.load().data ?? [];
    return (Object.keys(SERVICE_KIND_LABELS) as GuideServiceKind[])
      .map((kind) => ({
        kind,
        label: SERVICE_KIND_LABELS[kind],
        cards: services
          .filter((service) => service.service_kind === kind)
          .map((service): EntityCardItem => serviceCardItem(service)),
      }))
      .filter((group) => group.cards.length > 0);
  });

  constructor() {
    inject(GuidePageTitle).set('Services');
  }
}
