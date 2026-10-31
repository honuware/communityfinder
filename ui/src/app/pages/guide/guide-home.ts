import { Component, computed, inject } from '@angular/core';
import { MatProgressSpinnerModule } from '@angular/material/progress-spinner';
import { RouterLink } from '@angular/router';
import { of } from 'rxjs';
import { GUIDE_ACCESS } from '../../core/guide-access/guide-access';
import { GuideScene } from '../../core/types/guide.types';
import { listingCountsText } from '../../core/utils/guide-labels';
import { GuidePageTitle, loadEach } from './guide-page-state';

// /guide — the hub (GD1 §7). The scenes we cover ourselves as tiles; the
// link-out scenes as their own credited row ("sister scenes", GD1 §6.6); then
// the sections and the directories.
@Component({
  selector: 'app-guide-home',
  imports: [RouterLink, MatProgressSpinnerModule],
  templateUrl: './guide-home.html',
})
export class GuideHomePage {
  private access = inject(GUIDE_ACCESS);

  readonly load = loadEach(of(null), () => this.access.getScenes());
  readonly fullScenes = computed(() =>
    (this.load().data ?? []).filter((scene) => scene.coverage === 'full'),
  );
  readonly sisterScenes = computed(() =>
    (this.load().data ?? []).filter((scene) => scene.coverage === 'link_out'),
  );

  readonly sections: ReadonlyArray<{ label: string; path: string }> = [
    { label: 'Health', path: '/guide/sections/health' },
    { label: 'Services', path: '/guide/services' },
    { label: 'New to Seattle', path: '/guide/sections/new-to-seattle' },
    { label: 'Beat the Freeze', path: '/guide/sections/freeze' },
    { label: '18+', path: '/guide/sections/adult' },
  ];

  readonly directories: ReadonlyArray<{ label: string; note: string; path: string }> = [
    { label: 'Venues', note: 'Bars, clubs, cafés and spaces', path: '/guide/venues' },
    {
      label: 'Organizations',
      note: 'Leagues, choruses, recovery, faith and social groups',
      path: '/guide/orgs',
    },
    { label: 'Neighborhoods', note: 'Seattle and King County', path: '/guide/neighborhoods' },
    {
      label: 'Closed',
      note: "Venues and groups that aren't around anymore",
      path: '/guide/graveyard',
    },
  ];

  constructor() {
    inject(GuidePageTitle).set('Guide');
  }

  counts(scene: GuideScene): string {
    return listingCountsText(scene) || 'Being written';
  }
}
