import { Component, computed, inject } from '@angular/core';
import { MatButtonModule } from '@angular/material/button';
import { MatIconModule } from '@angular/material/icon';
import { MatProgressSpinnerModule } from '@angular/material/progress-spinner';
import { RouterLink } from '@angular/router';
import { GUIDE_ACCESS } from '../../core/guide-access/guide-access';
import { guideVocabularyLabel } from '../../core/services/guide-vocabularies.service';
import { ORGANIZATION_KIND_LABELS } from '../../core/utils/guide-labels';
import { AdultGate } from '../../shared/components/adult-gate/adult-gate';
import { FreshnessFooter } from '../../shared/components/freshness-footer/freshness-footer';
import { LinkOutCard } from '../../shared/components/link-out-card/link-out-card';
import { SceneChips } from '../../shared/components/scene-chips/scene-chips';
import { guideVocabularies, loadEach, routeParam, titleFromLoad } from './guide-page-state';

// /guide/orgs/:slug — GD1 §7's organization profile: name, kind and scene
// chips; what it is, when and where it meets; how to join; its links; the
// freshness footer. Closed and moved organizations keep their page with a
// banner; an 18+ organization (GD2 OQ11 (b)) sits behind the gate.
@Component({
  selector: 'app-organization-profile',
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
  templateUrl: './organization-profile.html',
})
export class OrganizationProfilePage {
  private access = inject(GUIDE_ACCESS);

  readonly load = loadEach(routeParam('slug'), (slug) => this.access.getOrganization(slug));
  readonly vocabularies = guideVocabularies();

  readonly organization = computed(() => this.load().data?.organization ?? null);
  readonly retired = computed(() => {
    const status = this.organization()?.status;
    return status === 'closed' || status === 'moved';
  });
  readonly kind = computed(() => {
    const kind = this.organization()?.org_kind;
    return kind ? (ORGANIZATION_KIND_LABELS[kind] ?? '') : '';
  });
  // '' for an organization with no one neighborhood: countywide, or it moves.
  readonly neighborhood = computed(() => {
    const slug = this.organization()?.neighborhood_slug;
    return slug ? guideVocabularyLabel(this.vocabularies().neighborhoods, slug) : '';
  });

  constructor() {
    titleFromLoad(this.load, (detail) => detail.organization.name, 'Organization not found');
  }
}
