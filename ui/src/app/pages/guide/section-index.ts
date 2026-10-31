import { Component, computed, inject } from '@angular/core';
import { toSignal } from '@angular/core/rxjs-interop';
import { MatButtonModule } from '@angular/material/button';
import { MatIconModule } from '@angular/material/icon';
import { MatProgressSpinnerModule } from '@angular/material/progress-spinner';
import { RouterLink } from '@angular/router';
import { GUIDE_ACCESS } from '../../core/guide-access/guide-access';
import { GuidePageSection, GuidePageSummary } from '../../core/types/guide.types';
import { guideVerifiedText } from '../../core/utils/guide-date.utils';
import { SECTION_LABELS } from '../../core/utils/guide-labels';
import { AdultGate } from '../../shared/components/adult-gate/adult-gate';
import { loadEach, notFound, routeParam, titleFromLoad } from './guide-page-state';

function isSection(value: string): value is GuidePageSection {
  return value in SECTION_LABELS;
}

// /guide/sections/:section — a section's published pages, by title, each with
// its "Verified" stamp. The 18+ section sits behind the gate. A section name
// that isn't one of GD1's seven is "not found" without asking the server.
@Component({
  selector: 'app-section-index',
  imports: [RouterLink, MatButtonModule, MatIconModule, MatProgressSpinnerModule, AdultGate],
  templateUrl: './section-index.html',
})
export class SectionIndexPage {
  private access = inject(GUIDE_ACCESS);
  private readonly section$ = routeParam('section');

  readonly section = toSignal(this.section$, { initialValue: '' });
  readonly load = loadEach(this.section$, (section) =>
    isSection(section)
      ? this.access.getGuidePagesBySection(section)
      : notFound<GuidePageSummary[]>(),
  );
  readonly label = computed(() => {
    const section = this.section();
    return isSection(section) ? SECTION_LABELS[section] : '';
  });

  constructor() {
    titleFromLoad(this.load, () => this.label(), 'Section not found');
  }

  verified(page: GuidePageSummary): string {
    return guideVerifiedText(page.last_verified_at_us);
  }
}
