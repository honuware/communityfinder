import { ColumnDataInfo, TableSchema } from '@honuware/ui/access';
import { MockRow } from '@honuware/ui/testing';
import { EventsMockData } from './events-mock-data';

// The events tables as the offline (mock) generic CRUD store holds them, so
// the bespoke event editor (/admin/events) works with no server: the pending
// queue, the venue and series pickers, the title search, the assignment
// checkboxes. The rows come from the same demo dataset the public pages show
// (events-mock-data). Only the columns the editor reads and writes are here.

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
    ],
    [
      { column_name: 'venue_id', parent_table_name: 'venues', parent_column_name: 'id' },
      { column_name: 'series_id', parent_table_name: 'series', parent_column_name: 'id' },
    ],
  ),
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
