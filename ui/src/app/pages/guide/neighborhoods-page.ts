import { Component, computed, inject } from '@angular/core';
import { MatButtonModule } from '@angular/material/button';
import { MatIconModule } from '@angular/material/icon';
import { MatProgressSpinnerModule } from '@angular/material/progress-spinner';
import { RouterLink } from '@angular/router';
import { of } from 'rxjs';
import { GUIDE_ACCESS } from '../../core/guide-access/guide-access';
import { GuideNeighborhood } from '../../core/types/guide.types';
import { listingCountsText, NEIGHBORHOOD_REGIONS } from '../../core/utils/guide-labels';
import { GuidePageTitle, loadEach } from './guide-page-state';

// /guide/neighborhoods — every neighborhood, grouped by GD1 §3's regions in
// its order, the Eastside a region of its own. Each shows what's listed there.
@Component({
  selector: 'app-neighborhoods-page',
  imports: [RouterLink, MatButtonModule, MatIconModule, MatProgressSpinnerModule],
  templateUrl: './neighborhoods-page.html',
})
export class NeighborhoodsPage {
  private access = inject(GUIDE_ACCESS);

  readonly load = loadEach(of(null), () => this.access.getNeighborhoods());

  readonly regions = computed(() => {
    const neighborhoods = this.load().data ?? [];
    return NEIGHBORHOOD_REGIONS.map(({ region, label }) => ({
      region,
      label,
      neighborhoods: neighborhoods.filter((neighborhood) => neighborhood.region === region),
    })).filter((group) => group.neighborhoods.length > 0);
  });

  constructor() {
    inject(GuidePageTitle).set('Neighborhoods');
  }

  counts(neighborhood: GuideNeighborhood): string {
    return listingCountsText(neighborhood);
  }
}
