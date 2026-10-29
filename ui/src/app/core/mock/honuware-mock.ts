import { DatabaseSchema } from '@honuware/ui/access';
import { MockRow, ProvideHonuwareAccessMockOptions } from '@honuware/ui/testing';
import { nowUs } from '../utils/events-date.utils';
import {
  buildEventsCrudSeedRows,
  EVENTS_CRUD_MOCK_DISPLAY_TEMPLATES,
  EVENTS_CRUD_MOCK_TABLES,
} from './events-crud-mock';
import { buildEventsMockData } from './events-mock-data';

// Offline dataset for the `local` (mock) build config: it makes @honuware/ui's
// in-memory access layer fully navigable with no server behind it. Real builds
// (`development`/`production`) ignore all of this and talk to the live API.
//
// It covers the two things the admin CRUD editor needs offline:
//   1. a signed-in admin (mock mode boots LOGGED OUT — sign in at /login with the
//      demo credentials below to reach /admin), and
//   2. a small get_db_schema stand-in + seed rows for the framework tables CF
//      exposes to admins, so the table picker + view/edit/new pages round-trip.

// A tiny schema mirroring the framework tables an admin edits. The real server
// returns this (and every table) from /api/get_db_schema; this is just enough to
// demo the editor. `display_templates` drive FK-picker / reference labels.
export const ADMIN_MOCK_SCHEMA: DatabaseSchema = {
  // role_assignments is a nested/junction table (mirrors the framework): it stays out
  // of the Manage Data picker (AdminPage filters nested out) and is edited via the
  // bespoke Roles & Permissions page, which reads it from `tables`/`seedRows` below.
  //
  // The events tables (EVENTS_CRUD_MOCK_TABLES) back the bespoke event editor
  // offline; their two assignment tables are nested, as on the server.
  root_tables: [
    'people',
    'roles',
    'role_assignments',
    'events',
    'venues',
    'series',
    'event_category_assignments',
    'event_scene_tag_assignments',
  ],
  nested_tables: ['role_assignments', 'event_category_assignments', 'event_scene_tag_assignments'],
  tables: [
    ...EVENTS_CRUD_MOCK_TABLES,
    {
      table_name: 'people',
      table_friendly_name: 'People',
      description: 'Registered community members and admins.',
      primary_key: 'id',
      foreign_keys: [],
      columns: [
        { column_name: 'id', type: 'SERIAL', primary_key: true, unique: false, nullable: false, label: 'ID', readonly: true },
        { column_name: 'email', type: 'VARCHAR', primary_key: false, unique: true, nullable: false, label: 'Email', html_input_type: 'email', required: true },
        { column_name: 'first_name', type: 'VARCHAR', primary_key: false, unique: false, nullable: false, label: 'First Name', required: true },
        { column_name: 'last_name', type: 'VARCHAR', primary_key: false, unique: false, nullable: false, label: 'Last Name', required: true },
      ],
    },
    {
      table_name: 'roles',
      table_friendly_name: 'Roles',
      description: 'Named permission bundles assignable to people.',
      primary_key: 'id',
      foreign_keys: [],
      columns: [
        { column_name: 'id', type: 'SERIAL', primary_key: true, unique: false, nullable: false, label: 'ID', readonly: true },
        { column_name: 'name', type: 'VARCHAR', primary_key: false, unique: true, nullable: false, label: 'Name', required: true },
        { column_name: 'description', type: 'VARCHAR', primary_key: false, unique: false, nullable: true, label: 'Description' },
      ],
    },
    {
      // "Roles & Permissions" — assigning people to roles. The person_id / role_id FK
      // columns resolve to names via the people / roles display_templates above.
      table_name: 'role_assignments',
      table_friendly_name: 'Role Assignments',
      description: 'Which roles each person holds.',
      primary_key: 'id',
      foreign_keys: [
        { column_name: 'person_id', parent_table_name: 'people', parent_column_name: 'id' },
        { column_name: 'role_id', parent_table_name: 'roles', parent_column_name: 'id' },
      ],
      columns: [
        { column_name: 'id', type: 'SERIAL', primary_key: true, unique: false, nullable: false, label: 'ID', readonly: true },
        { column_name: 'person_id', type: 'BIGINT', primary_key: false, unique: false, nullable: false, label: 'Person', required: true },
        { column_name: 'role_id', type: 'BIGINT', primary_key: false, unique: false, nullable: false, label: 'Role', required: true },
      ],
    },
  ],
  display_templates: {
    people: '{first_name} {last_name}',
    roles: '{name}',
    ...EVENTS_CRUD_MOCK_DISPLAY_TEMPLATES,
  },
  fk_picker_preload_threshold: 50,
};

export const ADMIN_MOCK_SEED_ROWS: Record<string, MockRow[]> = {
  people: [
    { id: '1', email: 'admin@communityfinder.local', first_name: 'Demo', last_name: 'Admin' },
    { id: '2', email: 'ada@example.com', first_name: 'Ada', last_name: 'Lovelace' },
    { id: '3', email: 'grace@example.com', first_name: 'Grace', last_name: 'Hopper' },
  ],
  roles: [
    { id: '1', name: 'admin', description: 'Administrator' },
    { id: '2', name: 'user', description: 'Standard member' },
  ],
  role_assignments: [
    { id: '1', person_id: '1', role_id: '1' }, // Demo Admin → admin
    { id: '2', person_id: '2', role_id: '2' }, // Ada → user
  ],
};

// The full offline access configuration handed to provideHonuwareAccessMock().
export const HONUWARE_MOCK_OPTIONS: ProvideHonuwareAccessMockOptions = {
  // Demo admin — sign in at /login (admin@communityfinder.local / changeme) to
  // exercise the admin editor offline. roles:['admin'] is what AuthService maps to
  // authData.isAdmin (auth.service.ts), which AdminGuard + the header key off.
  auth: {
    users: [
      {
        user: {
          person_id: 1,
          first_name: 'Demo',
          last_name: 'Admin',
          email: 'admin@communityfinder.local',
          created_at: '0',
          roles: ['admin'],
          // manage_events: the live seed grants it to the admin role (EV1 §2.6).
          permissions: ['admin_portal', 'manage_events'],
          must_change_password: false,
        },
        password: 'changeme',
      },
    ],
  },
  crud: {
    schema: ADMIN_MOCK_SCHEMA,
    // A getter, so the events rows are built only in mock mode, when the
    // store is created, relative to that moment.
    get seedRows(): Record<string, MockRow[]> {
      return {
        ...ADMIN_MOCK_SEED_ROWS,
        ...buildEventsCrudSeedRows(buildEventsMockData(nowUs())),
      };
    },
  },
};
