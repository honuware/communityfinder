import {
  GuideLinkPlatform,
  GuideNeighborhoodRegion,
  GuideOrganizationKind,
  GuidePageSection,
  GuidePlaceKind,
  GuideServiceKind,
} from '../types/guide.types';

// Display names for the guide's fixed lists (GD1 §4). The server sends the
// slugs; these are what a reader sees. Records, so a kind added to a type
// without a label here fails to compile.

export const ORGANIZATION_KIND_LABELS: Record<GuideOrganizationKind, string> = {
  'sports-league': 'Sports league',
  'chorus-performance': 'Chorus & performance',
  'club-hobby': 'Club & hobby',
  recovery: 'Recovery',
  faith: 'Faith',
  professional: 'Professional',
  'community-support': 'Community support',
  'social-meetup': 'Social meetup',
};

export const PLACE_KIND_LABELS: Record<GuidePlaceKind, string> = {
  'park-beach': 'Park & beach',
  landmark: 'Landmark',
  historical: 'Historical',
};

export const SERVICE_KIND_LABELS: Record<GuideServiceKind, string> = {
  'sti-testing': 'STI testing',
  prep: 'PrEP',
  'hiv-care': 'HIV care',
  'primary-care': 'Primary care',
  'mental-health': 'Mental health',
  'gender-affirming': 'Gender-affirming care',
  dental: 'Dental',
  'everyday-service': 'Everyday services',
};

// The guide's sections (GD1 §4), as section indexes and the guide home name
// them. `new-to` is GD9's "New to…" life-transition guides.
export const SECTION_LABELS: Record<GuidePageSection, string> = {
  scenes: 'Scenes',
  health: 'Health',
  services: 'Services',
  'new-to-seattle': 'New to Seattle',
  freeze: 'Beat the Freeze',
  'new-to': 'New to…',
  adult: '18+',
};

// GD1 §3's regions, in its order. The Eastside is a region of its own, never
// "other".
export const NEIGHBORHOOD_REGIONS: ReadonlyArray<{
  region: GuideNeighborhoodRegion;
  label: string;
}> = [
  { region: 'seattle-core', label: 'Seattle' },
  { region: 'seattle-south', label: 'South Seattle' },
  { region: 'seattle-north', label: 'North of Seattle' },
  { region: 'eastside', label: 'Eastside' },
  { region: 'south-king', label: 'South King County' },
];

// What a scene or neighborhood has listed: "2 venues · 1 organization"; ''
// when it has nothing yet.
export function listingCountsText(counts: {
  venue_count: number;
  organization_count: number;
  place_count: number;
}): string {
  const part = (count: number, one: string, many: string) =>
    count ? `${count} ${count === 1 ? one : many}` : '';
  return [
    part(counts.venue_count, 'venue', 'venues'),
    part(counts.organization_count, 'organization', 'organizations'),
    part(counts.place_count, 'place', 'places'),
  ]
    .filter((text) => text !== '')
    .join(' · ');
}

// GD1 §4: a link is always labeled with its platform.
export const LINK_PLATFORM_LABELS: Record<GuideLinkPlatform, string> = {
  discord: 'Discord',
  bluesky: 'Bluesky',
  instagram: 'Instagram',
  'facebook-group': 'Facebook group',
  meetup: 'Meetup',
  website: 'Website',
  other: 'Link',
};
