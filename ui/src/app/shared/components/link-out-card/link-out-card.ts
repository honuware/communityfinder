import { Component, computed, input } from '@angular/core';
import { MatIconModule } from '@angular/material/icon';
import { GuideLink, GuideLinkPlatform } from '../../../core/types/guide.types';
import { LINK_PLATFORM_LABELS } from '../../../core/utils/guide-labels';

// Material Icons has no brand marks, so each platform gets the nearest
// generic icon. The platform's name is always written out beside it.
const PLATFORM_ICONS: Record<GuideLinkPlatform, string> = {
  discord: 'forum',
  bluesky: 'cloud',
  instagram: 'photo_camera',
  'facebook-group': 'groups',
  meetup: 'event',
  website: 'language',
  other: 'link',
};

// GD1 §4/§7's "where this scene talks" card: one outbound link, labeled with
// its platform, opening in a new tab. Always a plain link, never an embedded
// feed. Profiles list these, and a link_out scene's landing is mostly these:
// "we point you to this scene's own curators, with credit".
@Component({
  selector: 'app-link-out-card',
  imports: [MatIconModule],
  templateUrl: './link-out-card.html',
  styleUrl: './link-out-card.scss',
})
export class LinkOutCard {
  readonly link = input.required<GuideLink>();

  readonly icon = computed(() => PLATFORM_ICONS[this.link().platform] ?? PLATFORM_ICONS.other);
  readonly platformLabel = computed(
    () => LINK_PLATFORM_LABELS[this.link().platform] ?? LINK_PLATFORM_LABELS.other,
  );
}
