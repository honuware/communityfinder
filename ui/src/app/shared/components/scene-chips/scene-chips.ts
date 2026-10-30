import { Component, computed, input } from '@angular/core';
import { RouterLink } from '@angular/router';
import { GuideScene } from '../../../core/types/guide.types';
import { guideVocabularyLabel } from '../../../core/services/guide-vocabularies.service';

// A listing's scene tags as chips, each a link to that scene's landing page
// (/guide/scenes/<slug>). Names come from the scene list the page passes in; a
// slug that isn't in it (or before it loads) shows as itself. Renders nothing
// when there are no tags.
@Component({
  selector: 'app-scene-chips',
  imports: [RouterLink],
  templateUrl: './scene-chips.html',
  styleUrl: './scene-chips.scss',
})
export class SceneChips {
  readonly slugs = input.required<string[]>();
  readonly scenes = input<GuideScene[]>([]);

  readonly chips = computed(() =>
    this.slugs().map((slug) => ({
      path: `/guide/scenes/${slug}`,
      label: guideVocabularyLabel(this.scenes(), slug),
    })),
  );
}
