import { Component, computed, inject } from '@angular/core';
import { toSignal } from '@angular/core/rxjs-interop';
import { MatButtonModule } from '@angular/material/button';
import { MatCheckboxModule } from '@angular/material/checkbox';
import { MatFormFieldModule } from '@angular/material/form-field';
import { MatIconModule } from '@angular/material/icon';
import { MatProgressSpinnerModule } from '@angular/material/progress-spinner';
import { MatSelectModule } from '@angular/material/select';
import { ActivatedRoute, ParamMap, Router, RouterLink } from '@angular/router';
import { map } from 'rxjs';
import { GUIDE_ACCESS } from '../../core/guide-access/guide-access';
import { GuideVenueFilters } from '../../core/types/guide.types';
import { EntityCard, venueCardItem } from '../../shared/components/entity-card/entity-card';
import { GuidePageTitle, guideVocabularies, loadEach } from './guide-page-state';

interface VenueDirectoryFilters {
  scene: string;
  neighborhood: string;
  includeClosed: boolean;
}

function filtersFromParams(params: ParamMap): VenueDirectoryFilters {
  return {
    scene: params.get('scene') ?? '',
    neighborhood: params.get('neighborhood') ?? '',
    includeClosed: params.get('include_closed') === 'true',
  };
}

function toAccessFilters(filters: VenueDirectoryFilters): GuideVenueFilters {
  return {
    scene: filters.scene,
    neighborhood: filters.neighborhood,
    include_closed: filters.includeClosed,
  };
}

// /guide/venues — every open venue, by name, with GD1 §7's cards. The filters
// (scene, neighborhood, and "include closed", which brings the graveyard's
// venues back in) live in the URL, as on the events list, so a filtered view
// can be shared.
@Component({
  selector: 'app-venue-directory',
  imports: [
    RouterLink,
    MatButtonModule,
    MatCheckboxModule,
    MatFormFieldModule,
    MatIconModule,
    MatProgressSpinnerModule,
    MatSelectModule,
    EntityCard,
  ],
  templateUrl: './venue-directory.html',
})
export class VenueDirectoryPage {
  private access = inject(GUIDE_ACCESS);
  private route = inject(ActivatedRoute);
  private router = inject(Router);

  private readonly filters$ = this.route.queryParamMap.pipe(map(filtersFromParams));
  readonly filters = toSignal(this.filters$, {
    initialValue: filtersFromParams(this.route.snapshot.queryParamMap),
  });
  readonly load = loadEach(this.filters$, (filters) =>
    this.access.getVenues(toAccessFilters(filters)),
  );
  readonly vocabularies = guideVocabularies();

  readonly cards = computed(() => (this.load().data ?? []).map(venueCardItem));
  readonly filtered = computed(() => {
    const { scene, neighborhood, includeClosed } = this.filters();
    return scene !== '' || neighborhood !== '' || includeClosed;
  });

  constructor() {
    inject(GuidePageTitle).set('Venues');
  }

  setFilter(key: 'scene' | 'neighborhood', value: string): void {
    this.navigate({ ...this.filters(), [key]: value });
  }

  setIncludeClosed(includeClosed: boolean): void {
    this.navigate({ ...this.filters(), includeClosed });
  }

  clearFilters(): void {
    this.navigate({ scene: '', neighborhood: '', includeClosed: false });
  }

  private navigate(filters: VenueDirectoryFilters): void {
    this.router.navigate([], {
      relativeTo: this.route,
      queryParams: {
        scene: filters.scene || null,
        neighborhood: filters.neighborhood || null,
        include_closed: filters.includeClosed ? 'true' : null,
      },
    });
  }
}
