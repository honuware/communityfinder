import { inject, Injectable } from '@angular/core';
import { catchError, forkJoin, Observable, of, shareReplay } from 'rxjs';
import { GUIDE_ACCESS } from '../guide-access/guide-access';
import { GuideNeighborhood, GuideScene } from '../types/guide.types';

// The display names behind a listing's scene and neighborhood slugs, as the
// guide's cards and chips show them. Stream B's own, read through the guide's
// seam rather than EventVocabulariesService (GD3's cross-stream pin).
export interface GuideVocabularies {
  scenes: GuideScene[];
  neighborhoods: GuideNeighborhood[];
}

export const EMPTY_GUIDE_VOCABULARIES: GuideVocabularies = {
  scenes: [],
  neighborhoods: [],
};

// A slug's display name, or the slug itself when it isn't in the list (a
// turned-off entry, or a list that failed to load).
export function guideVocabularyLabel(
  entries: { slug: string; display_name: string }[],
  slug: string,
): string {
  return entries.find((entry) => entry.slug === slug)?.display_name ?? slug;
}

// Fetched once per visit and shared by every guide page. A list that fails to
// load is empty: chips show slugs, but no page breaks over it.
//
// Not providedIn: 'root', because GUIDE_ACCESS moves into the guide routes'
// providers at §3.1 (GD3 OQ7 (d)), where a root service couldn't reach it.
// The guide routes provide this beside it.
@Injectable()
export class GuideVocabulariesService {
  private access = inject(GUIDE_ACCESS);

  readonly vocabularies$: Observable<GuideVocabularies> = forkJoin({
    scenes: this.access.getScenes().pipe(catchError(() => of<GuideScene[]>([]))),
    neighborhoods: this.access
      .getNeighborhoods()
      .pipe(catchError(() => of<GuideNeighborhood[]>([]))),
  }).pipe(shareReplay(1));
}
