import { ParamMap, Params } from '@angular/router';
import { EventVocabularies, vocabularyLabel } from '../../core/services/event-vocabularies.service';
import { PublicEvent } from '../../core/types/events.types';
import {
  formatEventDateLine,
  pacificDayKey,
  tonightRangeUs,
  UsRange,
  weekendRangeUs,
} from '../../core/utils/events-date.utils';

// The list page's view model: its filters as they sit in the URL, the
// client-side filtering over the one fetched window, and the grouping by day.
// The upcoming list fetches its window once and filters here, so toggling a
// filter is instant (EV2's fetch-strategy pin, after knottyyoga's
// CalendarService). The rules match the server's: an event's slug lists are
// already the effective ones, and the neighborhood is the venue's.

export type WhenPreset = 'tonight' | 'weekend';

export interface ListFilters {
  when: WhenPreset | '';
  category: string;
  scene: string;
  neighborhood: string;
}

export const EMPTY_LIST_FILTERS: ListFilters = {
  when: '',
  category: '',
  scene: '',
  neighborhood: '',
};

export function listFiltersFromParams(params: ParamMap): ListFilters {
  const when = params.get('when');
  return {
    when: when === 'tonight' || when === 'weekend' ? when : '',
    category: params.get('category') ?? '',
    scene: params.get('scene') ?? '',
    neighborhood: params.get('neighborhood') ?? '',
  };
}

// The filters as query params; unset ones are left out of the URL.
export function listFiltersToQueryParams(filters: ListFilters): Params {
  const params: Params = {};
  for (const key of ['when', 'category', 'scene', 'neighborhood'] as const) {
    if (filters[key]) {
      params[key] = filters[key];
    }
  }
  return params;
}

export function hasListFilters(filters: ListFilters): boolean {
  return Object.keys(listFiltersToQueryParams(filters)).length > 0;
}

export function presetRangeUs(when: WhenPreset, now: number): UsRange {
  return when === 'tonight' ? tonightRangeUs(now) : weekendRangeUs(now);
}

export function filterEvents(
  events: PublicEvent[],
  filters: ListFilters,
  now: number,
): PublicEvent[] {
  const range = filters.when ? presetRangeUs(filters.when, now) : null;
  return events.filter(
    (event) =>
      (!range || (event.starts_at_us >= range.fromUs && event.starts_at_us < range.toUs)) &&
      (!filters.category || event.category_slugs.includes(filters.category)) &&
      (!filters.scene || event.scene_tag_slugs.includes(filters.scene)) &&
      (!filters.neighborhood || event.neighborhood_slug === filters.neighborhood),
  );
}

// An event's (or a series') category and scene chips, each a link to that
// filtered list.
export interface TagChip {
  label: string;
  queryParams: Params;
}

export function tagChips(
  tagged: { category_slugs: string[]; scene_tag_slugs: string[] },
  vocabularies: EventVocabularies,
): TagChip[] {
  return [
    ...tagged.category_slugs.map((slug) => ({
      label: vocabularyLabel(vocabularies.categories, slug),
      queryParams: { category: slug },
    })),
    ...tagged.scene_tag_slugs.map((slug) => ({
      label: vocabularyLabel(vocabularies.sceneTags, slug),
      queryParams: { scene: slug },
    })),
  ];
}

// One Pacific day of the list (knottyyoga's ScheduleDayGroup).
export interface EventDayGroup {
  // "2026-08-14"
  key: string;
  // "Friday, Aug 14"
  dateLabel: string;
  events: PublicEvent[];
}

// Events (already in start order) grouped by the Pacific day they start on.
export function groupEventsByDay(events: PublicEvent[]): EventDayGroup[] {
  const groups: EventDayGroup[] = [];
  for (const event of events) {
    const key = pacificDayKey(event.starts_at_us);
    let group = groups[groups.length - 1];
    if (!group || group.key !== key) {
      group = { key, dateLabel: formatEventDateLine(event.starts_at_us), events: [] };
      groups.push(group);
    }
    group.events.push(event);
  }
  return groups;
}
