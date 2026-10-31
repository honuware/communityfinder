import { Component, computed, inject } from '@angular/core';
import { MatButtonModule } from '@angular/material/button';
import { MatIconModule } from '@angular/material/icon';
import { MatProgressSpinnerModule } from '@angular/material/progress-spinner';
import { RouterLink } from '@angular/router';
import { of } from 'rxjs';
import { GUIDE_ACCESS } from '../../core/guide-access/guide-access';
import {
  EntityCard,
  organizationCardItem,
  venueCardItem,
} from '../../shared/components/entity-card/entity-card';
import { GuidePageTitle, guideVocabularies, loadEach } from './guide-page-state';

// /guide/graveyard — the closure graveyard (GD1 §5): venues and organizations
// that have closed, each with its closure note and when it was last checked.
// History, not an error: they stay listed for the record and for anyone
// looking for them. Moved listings aren't here; they live on elsewhere.
@Component({
  selector: 'app-graveyard-page',
  imports: [RouterLink, MatButtonModule, MatIconModule, MatProgressSpinnerModule, EntityCard],
  templateUrl: './graveyard-page.html',
})
export class GraveyardPage {
  private access = inject(GUIDE_ACCESS);

  readonly load = loadEach(of(null), () => this.access.getGraveyard());
  readonly vocabularies = guideVocabularies();

  readonly venueCards = computed(() => (this.load().data?.venues ?? []).map(venueCardItem));
  readonly organizationCards = computed(() =>
    (this.load().data?.organizations ?? []).map(organizationCardItem),
  );

  constructor() {
    inject(GuidePageTitle).set('Closed venues and organizations');
  }
}
