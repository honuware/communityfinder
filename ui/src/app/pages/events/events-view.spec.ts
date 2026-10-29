import { convertToParamMap } from '@angular/router';
import { normalizeEvent, PublicEvent } from '../../core/types/events.types';
import { pacificWallClockToUs } from '../../core/utils/events-date.utils';
import { EMPTY_VOCABULARIES } from '../../core/services/event-vocabularies.service';
import {
  EMPTY_LIST_FILTERS,
  filterEvents,
  groupEventsByDay,
  hasListFilters,
  listFiltersFromParams,
  listFiltersToQueryParams,
  tagChips,
} from './events-view';

function event(id: number, startsAtUs: number, fields: Partial<PublicEvent> = {}): PublicEvent {
  return { ...normalizeEvent({ id, title: `Event ${id}`, starts_at_us: startsAtUs }), ...fields };
}

// Wed Aug 12, 2026, 6 PM Pacific.
const NOW = pacificWallClockToUs(2026, 8, 12, 18);

const EVENTS = [
  event(1, pacificWallClockToUs(2026, 8, 12, 21), { category_slugs: ['bar'] }),
  // 1 AM Thursday: still tonight.
  event(2, pacificWallClockToUs(2026, 8, 13, 1), { scene_tag_slugs: ['bear'] }),
  // 5 AM Thursday: no longer.
  event(3, pacificWallClockToUs(2026, 8, 13, 5), { neighborhood_slug: 'ballard' }),
  event(4, pacificWallClockToUs(2026, 8, 14, 22), { category_slugs: ['drag', 'bar'] }),
  event(5, pacificWallClockToUs(2026, 8, 16, 11), { neighborhood_slug: 'ballard' }),
  // The Monday after: past the weekend.
  event(6, pacificWallClockToUs(2026, 8, 17, 19)),
];

const ids = (events: PublicEvent[]) => events.map((e) => e.id);

describe('events-view', () => {
  it('reads the filters from the URL, ignoring an unknown preset', () => {
    expect(
      listFiltersFromParams(convertToParamMap({ when: 'tonight', scene: 'bear', other: 'x' })),
    ).toEqual({ ...EMPTY_LIST_FILTERS, when: 'tonight', scene: 'bear' });
    expect(listFiltersFromParams(convertToParamMap({ when: 'someday' })).when).toBe('');
  });

  it('writes only the set filters back to the URL', () => {
    expect(listFiltersToQueryParams(EMPTY_LIST_FILTERS)).toEqual({});
    expect(
      listFiltersToQueryParams({ ...EMPTY_LIST_FILTERS, when: 'weekend', category: 'drag' }),
    ).toEqual({
      when: 'weekend',
      category: 'drag',
    });
    expect(hasListFilters(EMPTY_LIST_FILTERS)).toBe(false);
    expect(hasListFilters({ ...EMPTY_LIST_FILTERS, neighborhood: 'ballard' })).toBe(true);
  });

  describe('filterEvents', () => {
    it('keeps tonight: now through 4 AM', () => {
      expect(ids(filterEvents(EVENTS, { ...EMPTY_LIST_FILTERS, when: 'tonight' }, NOW))).toEqual([
        1, 2,
      ]);
    });

    it('keeps this weekend: Friday through Sunday', () => {
      expect(ids(filterEvents(EVENTS, { ...EMPTY_LIST_FILTERS, when: 'weekend' }, NOW))).toEqual([
        4, 5,
      ]);
    });

    it('filters by category, scene and neighborhood, and combines them with a preset', () => {
      const only = (fields: Partial<typeof EMPTY_LIST_FILTERS>) =>
        ids(filterEvents(EVENTS, { ...EMPTY_LIST_FILTERS, ...fields }, NOW));
      expect(only({ category: 'bar' })).toEqual([1, 4]);
      expect(only({ scene: 'bear' })).toEqual([2]);
      expect(only({ neighborhood: 'ballard' })).toEqual([3, 5]);
      expect(only({ neighborhood: 'ballard', when: 'weekend' })).toEqual([5]);
      expect(only({ category: 'bar', when: 'tonight' })).toEqual([1]);
    });

    it('keeps everything with no filters', () => {
      expect(ids(filterEvents(EVENTS, EMPTY_LIST_FILTERS, NOW))).toEqual([1, 2, 3, 4, 5, 6]);
    });
  });

  it('groups by Pacific day, a 1 AM event under the day it falls on', () => {
    const groups = groupEventsByDay(EVENTS);
    expect(groups.map((group) => [group.key, group.dateLabel, ids(group.events)])).toEqual([
      ['2026-08-12', 'Wednesday, Aug 12', [1]],
      ['2026-08-13', 'Thursday, Aug 13', [2, 3]],
      ['2026-08-14', 'Friday, Aug 14', [4]],
      ['2026-08-16', 'Sunday, Aug 16', [5]],
      ['2026-08-17', 'Monday, Aug 17', [6]],
    ]);
  });

  it('labels the chips from the vocabularies, falling back to the slug', () => {
    const vocabularies = {
      ...EMPTY_VOCABULARIES,
      categories: [{ id: 1, slug: 'drag', display_name: 'Drag', description: '', sort_order: 10 }],
    };
    expect(tagChips({ category_slugs: ['drag'], scene_tag_slugs: ['bear'] }, vocabularies)).toEqual(
      [
        { label: 'Drag', queryParams: { category: 'drag' } },
        { label: 'bear', queryParams: { scene: 'bear' } },
      ],
    );
  });
});
