import { pacificWallClockToUs } from '../utils/events-date.utils';
import { buildEventsCrudSeedRows } from './events-crud-mock';
import { buildEventsMockData } from './events-mock-data';

// Wed Aug 12, 2026, 3:00 PM Pacific.
const NOW_US = pacificWallClockToUs(2026, 8, 12, 15);

describe('buildEventsCrudSeedRows', () => {
  const rows = buildEventsCrudSeedRows(buildEventsMockData(NOW_US));
  const categoryIdsOf = (eventId: string) =>
    rows['event_category_assignments']
      .filter((row) => row['event_id'] === eventId)
      .map((row) => row['category_id']);
  const sceneTagIdsOf = (eventId: string) =>
    rows['event_scene_tag_assignments']
      .filter((row) => row['event_id'] === eventId)
      .map((row) => row['scene_tag_id']);

  it('stores every event, the pending one included, with blank for no value', () => {
    expect(rows['events'].length).toBe(buildEventsMockData(NOW_US).events.length);
    const openMic = rows['events'].find((row) => row['id'] === '117')!;
    expect(openMic['status']).toBe('pending');
    const hike = rows['events'].find((row) => row['id'] === '110')!;
    expect(hike).toMatchObject({ venue_id: '', ends_at_us: '', series_id: '12' });
  });

  it("gives an event its own tag rows, but none to one inheriting its series'", () => {
    // Leather Night: bar (1) and leather (2) of its own.
    expect(categoryIdsOf('103')).toEqual(['1']);
    expect(sceneTagIdsOf('103')).toEqual(['2']);
    // Bear Happy Hour's instances inherit the series' bar / bear.
    expect(categoryIdsOf('107')).toEqual([]);
    expect(sceneTagIdsOf('107')).toEqual([]);
  });

  it('numbers the assignment rows uniquely', () => {
    for (const table of ['event_category_assignments', 'event_scene_tag_assignments']) {
      const ids = rows[table].map((row) => row['id']);
      expect(new Set(ids).size).toBe(ids.length);
    }
  });
});
