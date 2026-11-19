import { Component, computed, inject, input } from '@angular/core';
import { DomSanitizer, SafeResourceUrl } from '@angular/platform-browser';
import { VideoRef } from '../../../core/types/events.types';
import { videoEmbedUrl } from '../../../core/utils/video-links';

// CM3 §5.4: one YouTube or Vimeo video, shown as the provider's own player
// from the start. CM3 OQ23 reversed OQ4's click-to-play tile, because the
// player's still and one-tap play are the better experience.
// - The frame is titled "Video: <title>", fullscreen allowed, and not autoplaying.
// - It's lazy, so a visitor who never scrolls near it never loads the player.
// - Its address is built from the {provider, id} alone (video-links), and
//   trusted once per video: a fresh SafeResourceUrl on every change check
//   would make Angular reload the frame.
@Component({
  selector: 'app-video-embed',
  templateUrl: './video-embed.html',
  styleUrl: './video-embed.scss',
})
export class VideoEmbed {
  private sanitizer = inject(DomSanitizer);

  readonly video = input.required<VideoRef>();
  // What the video is of, for the frame's title.
  readonly title = input.required<string>();

  readonly frameTitle = computed(() => `Video: ${this.title()}`);
  // '' for an id of the wrong shape: then there's nothing to play.
  private readonly embedAddress = computed(() => videoEmbedUrl(this.video()));
  readonly playable = computed(() => this.embedAddress() !== '');
  // Trusted once per address, not per change check.
  readonly frameUrl = computed<SafeResourceUrl | null>(() => {
    const address = this.embedAddress();
    return address ? this.sanitizer.bypassSecurityTrustResourceUrl(address) : null;
  });
}
