import {
  GuideLinkPlatform,
  GuideOrganizationKind,
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
