import { Component, computed, input } from '@angular/core';
import { MatIconModule } from '@angular/material/icon';
import { RouterLink } from '@angular/router';
import {
  EMPTY_GUIDE_VOCABULARIES,
  GuideVocabularies,
  guideVocabularyLabel,
} from '../../../core/services/guide-vocabularies.service';
import {
  GuideFreshnessStatus,
  GuideOrganization,
  GuidePlace,
  GuideService,
  GuideVenue,
} from '../../../core/types/guide.types';
import { guideVerifiedText } from '../../../core/utils/guide-date.utils';
import {
  ORGANIZATION_KIND_LABELS,
  PLACE_KIND_LABELS,
  SERVICE_KIND_LABELS,
} from '../../../core/utils/guide-labels';
import { SceneChips } from '../scene-chips/scene-chips';

// One listing as a card, whatever its type. The *CardItem functions below make
// one from each listing type; every guide list (directories, scene and
// neighborhood pages, the graveyard) renders through them.
export interface EntityCardItem {
  name: string;
  // The listing's own page; null for a service (no detail pages at MVP).
  path: string | null;
  // Where a listing with no page of its own links out to; '' = nowhere.
  url: string;
  // A service's phone number, as entered; '' for everything else.
  phone: string;
  // "Sports league", "Park & beach", "STI testing"; '' for a venue.
  kindLabel: string;
  description: string;
  neighborhoodSlug: string;
  sceneTagSlugs: string[];
  adult: boolean;
  status: GuideFreshnessStatus;
  lastVerifiedAtUs: number | null;
  // What happened to a closed or moved listing ("Closed 2021, lease lost").
  closureNote: string;
}

export function venueCardItem(venue: GuideVenue): EntityCardItem {
  return {
    name: venue.name,
    path: `/guide/venues/${venue.slug}`,
    url: '',
    phone: '',
    kindLabel: '',
    description: venue.description,
    neighborhoodSlug: venue.neighborhood_slug,
    sceneTagSlugs: venue.scene_tag_slugs,
    adult: venue.adult,
    status: venue.status,
    lastVerifiedAtUs: venue.last_verified_at_us,
    closureNote: venue.closure_note,
  };
}

export function organizationCardItem(organization: GuideOrganization): EntityCardItem {
  return {
    name: organization.name,
    path: `/guide/orgs/${organization.slug}`,
    url: '',
    phone: '',
    kindLabel: ORGANIZATION_KIND_LABELS[organization.org_kind] ?? '',
    description: organization.description,
    neighborhoodSlug: organization.neighborhood_slug,
    sceneTagSlugs: organization.scene_tag_slugs,
    adult: organization.adult,
    status: organization.status,
    lastVerifiedAtUs: organization.last_verified_at_us,
    closureNote: organization.closure_note,
  };
}

export function placeCardItem(place: GuidePlace): EntityCardItem {
  return {
    name: place.name,
    path: `/guide/places/${place.slug}`,
    url: '',
    phone: '',
    kindLabel: PLACE_KIND_LABELS[place.place_kind] ?? '',
    description: place.description,
    neighborhoodSlug: place.neighborhood_slug,
    sceneTagSlugs: place.scene_tag_slugs,
    adult: place.adult,
    status: place.status,
    lastVerifiedAtUs: place.last_verified_at_us,
    closureNote: '',
  };
}

// Services have no page of their own: the card's name links out to the
// service's site, and it carries the phone number.
export function serviceCardItem(service: GuideService): EntityCardItem {
  return {
    name: service.name,
    path: null,
    url: service.url,
    phone: service.phone,
    kindLabel: SERVICE_KIND_LABELS[service.service_kind] ?? '',
    description: service.description,
    neighborhoodSlug: service.neighborhood_slug,
    sceneTagSlugs: [],
    adult: false,
    status: service.status,
    lastVerifiedAtUs: service.last_verified_at_us,
    closureNote: '',
  };
}

// The guide's list item (GD3 §2.5): name, a status badge when it isn't simply
// open, kind and neighborhood, a one-line description, scene chips, and the
// "Verified {Month Year}" stamp inline. Retired listings (closed, moved) show
// up only in the graveyard and the "include closed" directory, faded, with
// their closure note.
@Component({
  selector: 'app-entity-card',
  imports: [RouterLink, MatIconModule, SceneChips],
  templateUrl: './entity-card.html',
  styleUrl: './entity-card.scss',
})
export class EntityCard {
  readonly item = input.required<EntityCardItem>();
  readonly vocabularies = input<GuideVocabularies>(EMPTY_GUIDE_VOCABULARIES);

  readonly retired = computed(() => ['closed', 'moved'].includes(this.item().status));

  // The same markers as the freshness footer at the bottom of a page.
  readonly marker = computed(() => {
    switch (this.item().status) {
      case 'closed':
        return 'Closed';
      case 'moved':
        return 'Moved';
      case 'seasonal':
        return 'Seasonal';
      default:
        return '';
    }
  });

  // "Sports league · Capitol Hill"
  readonly meta = computed(() => {
    const item = this.item();
    const neighborhood = item.neighborhoodSlug
      ? guideVocabularyLabel(this.vocabularies().neighborhoods, item.neighborhoodSlug)
      : '';
    return [item.kindLabel, neighborhood].filter((part) => part !== '').join(' · ');
  });

  readonly verifiedText = computed(() => guideVerifiedText(this.item().lastVerifiedAtUs));

  // "206-555-0601" for a ten-digit US number; anything else as entered.
  readonly phoneText = computed(() => {
    const phone = this.item().phone;
    const digits = phone.replace(/\D/g, '');
    return digits.length === 10
      ? `${digits.slice(0, 3)}-${digits.slice(3, 6)}-${digits.slice(6)}`
      : phone;
  });
  readonly phoneHref = computed(() => `tel:${this.item().phone.replace(/[^\d+]/g, '')}`);
}
