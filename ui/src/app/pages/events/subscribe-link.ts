import { Component, computed, DOCUMENT, inject, input } from '@angular/core';
import { Clipboard } from '@angular/cdk/clipboard';
import { MatButtonModule } from '@angular/material/button';
import { MatIconModule } from '@angular/material/icon';
import { HONUWARE_API_BASE } from '@honuware/ui/access';
import { ToastService } from '@honuware/ui/foundation';
import {
  absoluteUrl,
  EventsFeedFilters,
  eventsFeedPath,
  webcalUrl,
} from '../../core/utils/event-links';

// "Subscribe to this view": the calendar feed for the filters on screen
// (/api/events/feed.ics?…), so a calendar app shows new matching events as
// they're added. Subscribe opens the calendar app (webcal://); Copy link is
// for Google Calendar, which only subscribes from a pasted address.
@Component({
  selector: 'app-subscribe-link',
  imports: [MatButtonModule, MatIconModule],
  templateUrl: './subscribe-link.html',
  styleUrl: './subscribe-link.scss',
})
export class SubscribeLink {
  private apiBase = inject(HONUWARE_API_BASE);
  private document = inject(DOCUMENT);
  private clipboard = inject(Clipboard);
  private toast = inject(ToastService);

  readonly filters = input<EventsFeedFilters>({});
  // What the feed holds, e.g. "Drag events" or "all events".
  readonly label = input('all events');

  readonly feedUrl = computed(() =>
    absoluteUrl(this.document.location.origin, eventsFeedPath(this.apiBase, this.filters())),
  );
  readonly subscribeUrl = computed(() => webcalUrl(this.feedUrl()));

  copy(): void {
    if (this.clipboard.copy(this.feedUrl())) {
      this.toast.success('Calendar link copied');
    } else {
      this.toast.error('Could not copy the link');
    }
  }
}
