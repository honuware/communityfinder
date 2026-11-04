import { pacificWallClockToUs } from '../utils/events-date.utils';
import { buildEventsCrudSeedRows, buildSubmissionsCrudSeedRows } from './events-crud-mock';
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

describe('buildSubmissionsCrudSeedRows', () => {
  const data = buildEventsMockData(NOW_US);
  const rows = buildSubmissionsCrudSeedRows(data, NOW_US);

  it('seeds two submissions from the admin seed’s people', () => {
    expect(
      rows['event_submissions'].map((row) => [row['id'], row['submitted_by_person_id']]),
    ).toEqual([
      ['1', '2'],
      ['2', '3'],
    ]);
    expect(rows['event_submissions'][0]).toMatchObject({
      relationship: 'organizer',
      regular_thing: 't',
      submitter_note: 'We meet every other week at the back tables.',
    });
  });

  it('puts their dates in the queue as submitted, pending events', () => {
    expect(rows['events'].map((row) => [row['id'], row['submission_id']])).toEqual([
      ['201', '1'],
      ['202', '1'],
      ['203', '2'],
    ]);
    for (const row of rows['events']) {
      expect(row).toMatchObject({ origin: 'user_submitted', status: 'pending' });
    }
    // Queer Craft Night: a week out and three weeks out, at 7 PM, with no link.
    expect(rows['events'][0]).toMatchObject({
      starts_at_us: String(pacificWallClockToUs(2026, 8, 18, 19)),
      url: '',
    });
    expect(rows['events'][1]['starts_at_us']).toBe(String(pacificWallClockToUs(2026, 9, 1, 19)));
  });

  it('puts "Leather Night!" an hour after the listed one, at the same venue', () => {
    const leatherNight = data.events.find(({ event }) => event.id === 103)!.event;
    expect(rows['events'][2]).toMatchObject({
      title: 'Leather Night!',
      venue_id: String(leatherNight.venue_id),
      starts_at_us: String(leatherNight.starts_at_us + 3_600_000_000),
    });
  });

  it('keeps the ids clear of the dataset’s own', () => {
    const datasetIds = new Set(buildEventsCrudSeedRows(data)['events'].map((row) => row['id']));
    expect(rows['events'].some((row) => datasetIds.has(row['id']))).toBe(false);
  });
});
