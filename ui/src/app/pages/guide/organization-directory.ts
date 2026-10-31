import { Component, computed, inject } from '@angular/core';
import { toSignal } from '@angular/core/rxjs-interop';
import { MatButtonModule } from '@angular/material/button';
import { MatFormFieldModule } from '@angular/material/form-field';
import { MatIconModule } from '@angular/material/icon';
import { MatProgressSpinnerModule } from '@angular/material/progress-spinner';
import { MatSelectModule } from '@angular/material/select';
import { ActivatedRoute, ParamMap, Router, RouterLink } from '@angular/router';
import { map } from 'rxjs';
import { GUIDE_ACCESS } from '../../core/guide-access/guide-access';
import { GuideOrganizationKind } from '../../core/types/guide.types';
import { ORGANIZATION_KIND_LABELS } from '../../core/utils/guide-labels';
import { EntityCard, organizationCardItem } from '../../shared/components/entity-card/entity-card';
import { GuidePageTitle, guideVocabularies, loadEach } from './guide-page-state';

interface OrganizationDirectoryFilters {
  kind: GuideOrganizationKind | '';
  scene: string;
  neighborhood: string;
}

function isOrganizationKind(value: string): value is GuideOrganizationKind {
  return value in ORGANIZATION_KIND_LABELS;
}

function filtersFromParams(params: ParamMap): OrganizationDirectoryFilters {
  const kind = params.get('kind') ?? '';
  return {
    kind: isOrganizationKind(kind) ? kind : '',
    scene: params.get('scene') ?? '',
    neighborhood: params.get('neighborhood') ?? '',
  };
}

// /guide/orgs — the organization directory, the data side of Beat the Freeze:
// leagues, choruses, recovery and faith groups, professional and social ones,
// the ways to meet people that aren't a bar. Filters by kind (one button per
// kind), scene and neighborhood, all kept in the URL.
@Component({
  selector: 'app-organization-directory',
  imports: [
    RouterLink,
    MatButtonModule,
    MatFormFieldModule,
    MatIconModule,
    MatProgressSpinnerModule,
    MatSelectModule,
    EntityCard,
  ],
  templateUrl: './organization-directory.html',
})
export class OrganizationDirectoryPage {
  private access = inject(GUIDE_ACCESS);
  private route = inject(ActivatedRoute);
  private router = inject(Router);

  readonly kinds = Object.entries(ORGANIZATION_KIND_LABELS).map(([kind, label]) => ({
    kind: kind as GuideOrganizationKind,
    label,
  }));

  private readonly filters$ = this.route.queryParamMap.pipe(map(filtersFromParams));
  readonly filters = toSignal(this.filters$, {
    initialValue: filtersFromParams(this.route.snapshot.queryParamMap),
  });
  readonly load = loadEach(this.filters$, ({ kind, scene, neighborhood }) =>
    this.access.getOrganizations({ kind: kind || undefined, scene, neighborhood }),
  );
  readonly vocabularies = guideVocabularies();

  readonly cards = computed(() => (this.load().data ?? []).map(organizationCardItem));
  readonly filtered = computed(() => {
    const { kind, scene, neighborhood } = this.filters();
    return kind !== '' || scene !== '' || neighborhood !== '';
  });

  constructor() {
    inject(GuidePageTitle).set('Organizations');
  }

  // Picking the kind that's already on turns it off.
  toggleKind(kind: GuideOrganizationKind): void {
    this.navigate({ ...this.filters(), kind: this.filters().kind === kind ? '' : kind });
  }

  setFilter(key: 'scene' | 'neighborhood', value: string): void {
    this.navigate({ ...this.filters(), [key]: value });
  }

  clearFilters(): void {
    this.navigate({ kind: '', scene: '', neighborhood: '' });
  }

  private navigate(filters: OrganizationDirectoryFilters): void {
    this.router.navigate([], {
      relativeTo: this.route,
      queryParams: {
        kind: filters.kind || null,
        scene: filters.scene || null,
        neighborhood: filters.neighborhood || null,
      },
    });
  }
}
