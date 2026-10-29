import { Component, computed, input } from '@angular/core';
import { FreshnessStatus } from '../../../core/types/events.types';
import { formatMonthYear } from '../../../core/utils/events-date.utils';

// GD1 §7's freshness footer, which ends every listing page: "Verified
// {Month Year}" from the last human check (never hidden when stale — the
// honest date is the trust signal), plus a marker when the listing isn't
// simply open. The series page uses it now; GD3's guide pages can too.
@Component({
  selector: 'app-freshness-footer',
  templateUrl: './freshness-footer.html',
  styleUrl: './freshness-footer.scss',
})
export class FreshnessFooter {
  readonly status = input.required<FreshnessStatus>();
  readonly lastVerifiedAtUs = input<number | null>(null);
  // What a closed listing is called: a series has "Ended", a venue "Closed".
  readonly closedLabel = input('Closed');

  readonly verifiedText = computed(() => {
    const verifiedAtUs = this.lastVerifiedAtUs();
    return verifiedAtUs === null ? 'Not yet verified' : `Verified ${formatMonthYear(verifiedAtUs)}`;
  });

  readonly marker = computed(() => {
    switch (this.status()) {
      case 'closed':
        return this.closedLabel();
      case 'moved':
        return 'Moved';
      case 'seasonal':
        return 'Seasonal';
      default:
        return '';
    }
  });
}
