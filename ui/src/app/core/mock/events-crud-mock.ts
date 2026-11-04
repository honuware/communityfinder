import { ColumnDataInfo, TableSchema } from '@honuware/ui/access';
import { MockRow } from '@honuware/ui/testing';
import { pacificDateTime, pacificWallClockToUs } from '../utils/events-date.utils';
import { US_PER_DAY } from '../types/events.types';
import { EventsMockData } from './events-mock-data';

// The events tables as the offline (mock) generic CRUD store holds them, so
// the bespoke event editor (/admin/events) works with no server: the pending
// queue, the venue and series pickers, the title search, the assignment
// checkboxes, and submitted events grouped by submission (EV3). The rows come
// from the same demo dataset the public pages show (events-mock-data), plus
// two sample submissions. Only the columns the pages read and write are here.

function column(
  columnName: string,
  type: string,
  extra: Partial<ColumnDataInfo> = {},
): ColumnDataInfo {
  return {
    column_name: columnName,
    type,
    primary_key: columnName === 'id',
    unique: false,
    nullable: false,
    ...extra,
  };
}

function table(
  tableName: string,
  friendlyName: string,
  columns: ColumnDataInfo[],
  foreignKeys: TableSchema['foreign_keys'] = [],
): TableSchema {
  return {
    table_name: tableName,
    table_friendly_name: friendlyName,
    description: '',
    primary_key: 'id',
    foreign_keys: foreignKeys,
    columns,
  };
}

const ID = column('id', 'BIGSERIAL', { readonly: true });

export const EVENTS_CRUD_MOCK_TABLES: TableSchema[] = [
  table('venues', 'Venues', [ID, column('name', 'VARCHAR'), column('address', 'VARCHAR')]),
  table('series', 'Series', [ID, column('title', 'VARCHAR'), column('slug', 'VARCHAR')]),
  table(
    'events',
    'Events',
    [
      ID,
      column('title', 'VARCHAR'),
      column('description', 'VARCHAR'),
      column('starts_at_us', 'BIGINT'),
      column('ends_at_us', 'BIGINT', { nullable: true }),
      column('venue_id', 'BIGINT', { nullable: true }),
      column('location_text', 'VARCHAR'),
      column('url', 'VARCHAR'),
      column('cost_text', 'VARCHAR'),
      column('series_id', 'BIGINT', { nullable: true }),
      column('origin', 'VARCHAR'),
      column('status', 'VARCHAR'),
      column('created_by_person_id', 'BIGINT', { nullable: true }),
      column('review_notes', 'VARCHAR'),
      column('submission_id', 'BIGINT', { nullable: true, readonly: true }),
    ],
    [
      { column_name: 'venue_id', parent_table_name: 'venues', parent_column_name: 'id' },
      { column_name: 'series_id', parent_table_name: 'series', parent_column_name: 'id' },
      {
        column_name: 'submission_id',
        parent_table_name: 'event_submissions',
        parent_column_name: 'id',
      },
    ],
  ),
  table('event_submissions', 'Event Submissions', [
    ID,
    column('submitted_by_person_id', 'BIGINT', { readonly: true }),
    column('submitter_note', 'VARCHAR'),
    column('relationship', 'VARCHAR'),
    column('regular_thing', 'BOOLEAN'),
    column('reject_reason', 'VARCHAR', { nullable: true }),
    column('message_to_submitter', 'VARCHAR'),
    column('decision_notified_us', 'BIGINT', { nullable: true }),
    column('created_us', 'BIGINT', { readonly: true }),
    column('updated_us', 'BIGINT', { readonly: true }),
  ]),
  table(
    'event_category_assignments',
    'Event Categories',
    [ID, column('event_id', 'BIGINT'), column('category_id', 'BIGINT')],
    [{ column_name: 'event_id', parent_table_name: 'events', parent_column_name: 'id' }],
  ),
  table(
    'event_scene_tag_assignments',
    'Event Scene Tags',
    [ID, column('event_id', 'BIGINT'), column('scene_tag_id', 'BIGINT')],
    [{ column_name: 'event_id', parent_table_name: 'events', parent_column_name: 'id' }],
  ),
];

export const EVENTS_CRUD_MOCK_DISPLAY_TEMPLATES: Record<string, string> = {
  events: '{title}',
  venues: '{name}',
  series: '{title}',
  event_submissions: 'Submission #{id}',
};

function sameSlugs(a: string[], b: string[]): boolean {
  return a.length === b.length && a.every((slug) => b.includes(slug));
}

// The dataset as table rows. An event in a series whose slugs are the
// series' own is inheriting them (the mock data already resolved them), so it
// gets no assignment rows of its own, just as the server would store it.
export function buildEventsCrudSeedRows(data: EventsMockData): Record<string, MockRow[]> {
  const categoryIds = new Map(data.categories.map((category) => [category.slug, category.id]));
  const sceneTagIds = new Map(data.sceneTags.map((tag) => [tag.slug, tag.id]));
  const seriesById = new Map(data.series.map((series) => [series.id, series]));
  const categoryRows: MockRow[] = [];
  const sceneTagRows: MockRow[] = [];

  const events = data.events.map(({ status, event }) => {
    const series = event.series_id !== null ? seriesById.get(event.series_id) : undefined;
    const ownCategories =
      series && sameSlugs(event.category_slugs, series.category_slugs) ? [] : event.category_slugs;
    const ownSceneTags =
      series && sameSlugs(event.scene_tag_slugs, series.scene_tag_slugs)
        ? []
        : event.scene_tag_slugs;
    for (const slug of ownCategories) {
      categoryRows.push({
        id: String(categoryRows.length + 1),
        event_id: String(event.id),
        category_id: String(categoryIds.get(slug) ?? ''),
      });
    }
    for (const slug of ownSceneTags) {
      sceneTagRows.push({
        id: String(sceneTagRows.length + 1),
        event_id: String(event.id),
        scene_tag_id: String(sceneTagIds.get(slug) ?? ''),
      });
    }
    return {
      id: String(event.id),
      title: event.title,
      description: event.description,
      starts_at_us: String(event.starts_at_us),
      ends_at_us: event.ends_at_us === null ? '' : String(event.ends_at_us),
      venue_id: event.venue_id === null ? '' : String(event.venue_id),
      location_text: event.location_text,
      url: event.url,
      cost_text: event.cost_text,
      series_id: event.series_id === null ? '' : String(event.series_id),
      origin: event.origin,
      status,
      created_by_person_id: '',
      review_notes: '',
      submission_id: '',
    };
  });

  return {
    venues: data.venues.map((venue) => ({
      id: String(venue.id),
      name: venue.name,
      address: venue.address,
    })),
    series: data.series.map((series) => ({
      id: String(series.id),
      title: series.title,
      slug: series.slug,
    })),
    events,
    event_category_assignments: categoryRows,
    event_scene_tag_assignments: sceneTagRows,
  };
}

const HOUR_US = 3_600 * 1_000_000;

// Two sample submissions waiting for review, so the queue's grouping and flags
// show offline (EV3 §5.5). Their dates are pending, so the public pages never
// show them.
//   #1: Ada's "Queer Craft Night", two dates, a regular thing with no link.
//   #2: Grace's "Leather Night!", an hour after the listed Leather Night at
//       the same venue, so the queue flags it as a possible duplicate.
// The people are the admin seed's (people 2 and 3).
export function buildSubmissionsCrudSeedRows(
  data: EventsMockData,
  now: number,
): Record<string, MockRow[]> {
  const today = pacificDateTime(now);
  const at = (dayOffset: number, hour: number) =>
    pacificWallClockToUs(today.year, today.month, today.day + dayOffset, hour);
  const leatherNightStartsAtUs =
    data.events.find(({ event }) => event.id === 103)?.event.starts_at_us ?? at(2, 21);

  const submissions: MockRow[] = [
    {
      id: '1',
      submitted_by_person_id: '2',
      submitter_note: 'We meet every other week at the back tables.',
      relationship: 'organizer',
      regular_thing: 't',
      reject_reason: '',
      message_to_submitter: '',
      decision_notified_us: '',
      created_us: String(now - 2 * US_PER_DAY),
      updated_us: String(now - 2 * US_PER_DAY),
    },
    {
      id: '2',
      submitted_by_person_id: '3',
      submitter_note: '',
      relationship: 'attendee',
      regular_thing: 'f',
      reject_reason: '',
      message_to_submitter: '',
      decision_notified_us: '',
      created_us: String(now - US_PER_DAY),
      updated_us: String(now - US_PER_DAY),
    },
  ];

  const submitted = (
    id: number,
    submissionId: string,
    personId: string,
    fields: {
      title: string;
      description: string;
      startsAtUs: number;
      venueId: number;
      url: string;
      costText: string;
    },
  ): MockRow => ({
    id: String(id),
    title: fields.title,
    description: fields.description,
    starts_at_us: String(fields.startsAtUs),
    ends_at_us: String(fields.startsAtUs + 2 * HOUR_US),
    venue_id: String(fields.venueId),
    location_text: '',
    url: fields.url,
    cost_text: fields.costText,
    series_id: '',
    origin: 'user_submitted',
    status: 'pending',
    created_by_person_id: personId,
    review_notes: '',
    submission_id: submissionId,
  });
  const craftNight = {
    title: 'Queer Craft Night',
    description: 'Bring a project, or start one with the yarn and fabric we bring.',
    venueId: 3,
    url: '',
    costText: 'Free',
  };

  return {
    event_submissions: submissions,
    events: [
      submitted(201, '1', '2', { ...craftNight, startsAtUs: at(6, 19) }),
      submitted(202, '1', '2', { ...craftNight, startsAtUs: at(20, 19) }),
      submitted(203, '2', '3', {
        title: 'Leather Night!',
        description: 'Gear night. Coat check at the door.',
        startsAtUs: leatherNightStartsAtUs + HOUR_US,
        venueId: 1,
        url: 'https://example.org/leather-night',
        costText: '$10 cover',
      }),
    ],
  };
}
