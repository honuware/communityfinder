import { inject, Injectable } from '@angular/core';
import { catchError, forkJoin, Observable, of, shareReplay } from 'rxjs';
import { COMMUNITY_ACCESS } from '../community-access/community-access';
import { Category, Neighborhood, SceneTag } from '../types/events.types';

// The three shared vocabularies (GD1 §1–§3), as the events pages use them:
// the filter bar's choices, and the display names behind an event's slugs.
export interface EventVocabularies {
  categories: Category[];
  sceneTags: SceneTag[];
  neighborhoods: Neighborhood[];
}

export const EMPTY_VOCABULARIES: EventVocabularies = {
  categories: [],
  sceneTags: [],
  neighborhoods: [],
};

// A slug's display name, or the slug itself when it isn't in the list (a
// turned-off entry, or a list that failed to load).
export function vocabularyLabel(entries: { slug: string; display_name: string }[], slug: string) {
  return entries.find((entry) => entry.slug === slug)?.display_name ?? slug;
}

// Fetched once per visit and shared by every events page. A list that fails
// to load is empty: filters lose their choices and chips show slugs, but no
// page breaks over it.
@Injectable({ providedIn: 'root' })
export class EventVocabulariesService {
  private access = inject(COMMUNITY_ACCESS);

  readonly vocabularies$: Observable<EventVocabularies> = forkJoin({
    categories: this.access.getCategories().pipe(catchError(() => of<Category[]>([]))),
    sceneTags: this.access.getSceneTags().pipe(catchError(() => of<SceneTag[]>([]))),
    neighborhoods: this.access.getNeighborhoods().pipe(catchError(() => of<Neighborhood[]>([]))),
  }).pipe(shareReplay(1));
}
