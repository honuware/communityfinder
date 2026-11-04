import { ComponentFixture, TestBed } from '@angular/core/testing';
import { By } from '@angular/platform-browser';
import { provideNoopAnimations } from '@angular/platform-browser/animations';
import { provideRouter } from '@angular/router';
import { firstValueFrom } from 'rxjs';
import { vi } from 'vitest';
import { CrudAccess } from '@honuware/ui/access';
import { AuthService } from '@honuware/ui/auth';
import { SimpleDateComponent } from '@honuware/ui/controls';
import { provideHonuwareAccessMock } from '@honuware/ui/testing';
import { HONUWARE_MOCK_OPTIONS } from '../../core/mock/honuware-mock';
import { dataResultsToRecords } from '../../core/types/events.types';
import { pacificWallClockToUs } from '../../core/utils/events-date.utils';
import { EditRequest, EventEditor, SavedEvent } from './event-editor';
import { EMPTY_EVENT_FORM } from './manage-events-form';

// Wed Aug 12, 2026, 3:00 PM Pacific: the mock dataset's reference week.
const NOW_US = pacificWallClockToUs(2026, 8, 12, 15);

// In the seed: Leather Night (Fri 9 PM – Sat 2 AM at The Lantern Room) has
// category bar (1) and scene leather (2) of its own.
const LEATHER_NIGHT = '103';
const BAR = 1;
const COMMUNITY = 12;
const BEAR = 1;
const LEATHER = 2;
const ELDERS = 5;

// The checkboxes the page would hand down (only their ids matter here).
const CATEGORIES = [
  { id: BAR, label: 'Bar' },
  { id: COMMUNITY, label: 'Community' },
];
const SCENE_TAGS = [
  { id: BEAR, label: 'Bear' },
  { id: LEATHER, label: 'Leather' },
  { id: ELDERS, label: 'Elders' },
];

const ADMIN = {
  isAuth: true,
  personId: 1,
  firstName: 'Demo',
  lastName: 'Admin',
  email: 'admin@communityfinder.local',
  createdAt: '0',
  isAdmin: true,
  roles: ['admin'],
  permissions: ['admin_portal', 'manage_events'],
  mustChangePassword: false,
};

// An editor on the offline CRUD store: a new event's, or given a request,
// that event's.
function create(request: EditRequest | null = null) {
  let crud!: CrudAccess;
  TestBed.configureTestingModule({
    providers: [
      provideRouter([]),
      provideNoopAnimations(),
      provideHonuwareAccessMock(HONUWARE_MOCK_OPTIONS, (mocks) => (crud = mocks.crud)),
      { provide: AuthService, useValue: { authData: ADMIN } },
    ],
  });
  const fixture = TestBed.createComponent(EventEditor);
  fixture.componentRef.setInput('categories', CATEGORIES);
  fixture.componentRef.setInput('sceneTags', SCENE_TAGS);
  fixture.componentRef.setInput('request', request);
  const saved: SavedEvent[] = [];
  fixture.componentInstance.saved.subscribe((event) => saved.push(event));
  fixture.detectChanges();
  return {
    crud,
    fixture,
    editor: fixture.componentInstance,
    element: fixture.nativeElement as HTMLElement,
    saved,
  };
}

async function eventRow(crud: CrudAccess, id: string) {
  return dataResultsToRecords(await firstValueFrom(crud.getRow('events', 'id', id)))[0];
}

async function eventRows(crud: CrudAccess) {
  return dataResultsToRecords(await firstValueFrom(crud.getTableRows('events')));
}

async function assignedTagIds(crud: CrudAccess, table: string, tagColumn: string, eventId: string) {
  const result = await firstValueFrom(
    crud.getFilteredTableRows(table, 'id', true, 500, 0, [
      { column_name: 'event_id', column_value: eventId },
    ]),
  );
  return dataResultsToRecords(result).map((row) => Number(row[tagColumn]));
}

const categoriesOf = (crud: CrudAccess, eventId: string) =>
  assignedTagIds(crud, 'event_category_assignments', 'category_id', eventId);
const sceneTagsOf = (crud: CrudAccess, eventId: string) =>
  assignedTagIds(crud, 'event_scene_tag_assignments', 'scene_tag_id', eventId);

function pickers(fixture: ComponentFixture<EventEditor>): SimpleDateComponent[] {
  return fixture.debugElement
    .queryAll(By.directive(SimpleDateComponent))
    .map((picker) => picker.componentInstance as SimpleDateComponent);
}

function pickerInputs(element: HTMLElement): HTMLInputElement[] {
  return Array.from(element.querySelectorAll('hw-simple-date input'));
}

// What a picker box shows, with the time's narrow space before AM/PM made plain.
function shown(input: HTMLInputElement): string {
  return input.value.replace(/\s+/g, ' ');
}

function typeInto(fixture: ComponentFixture<EventEditor>, input: HTMLInputElement, value: string) {
  input.value = value;
  input.dispatchEvent(new Event('input', { bubbles: true }));
  fixture.detectChanges();
}

function text(element: Element | null): string {
  return (element?.textContent ?? '').replace(/\s+/g, ' ').trim();
}

describe('EventEditor', () => {
  beforeEach(() => {
    vi.useFakeTimers({ toFake: ['Date'] });
    vi.setSystemTime(new Date(NOW_US / 1000));
  });
  afterEach(() => {
    vi.useRealTimers();
    vi.restoreAllMocks();
  });

  describe('dates and times', () => {
    it('picks them with the honuware pickers, start then end', () => {
      const { fixture, element } = create();
      expect(pickers(fixture).map((picker) => picker.type)).toEqual([
        'date',
        'time',
        'time',
        'date',
      ]);
      expect(pickers(fixture)[1].timeIncrementMinutes).toBe(15);
      const labels = Array.from(element.querySelectorAll('hw-simple-date mat-label')).map(text);
      expect(labels).toEqual(['Start date', 'Start time', 'End time', 'End date']);
      expect(element.querySelectorAll('mat-datepicker-toggle').length).toBe(2);
      expect(element.querySelectorAll('mat-timepicker-toggle').length).toBe(2);
      expect(element.querySelector('input[type="date"], input[type="time"]')).toBeNull();
    });

    it('shows the opened event’s times, and keeps what is typed', () => {
      const { editor, fixture, element } = create({ id: LEATHER_NIGHT });
      // 9 PM – 2 AM: the end time alone says it, so the end date is blank and
      // the note says when it ends.
      expect(pickerInputs(element).map(shown)).toEqual(['8/14/2026', '9:00 PM', '2:00 AM', '']);
      expect(text(element.querySelector('.event-editor__end-note'))).toBe(
        'Ends the next day: Sat, Aug 15, 2:00 AM.',
      );
      const [startDate, startTime] = pickerInputs(element);
      typeInto(fixture, startDate, '8/21/2026');
      typeInto(fixture, startTime, '10:00 PM');
      expect(editor.form.getRawValue()).toMatchObject({
        startDate: '2026-08-21',
        startTime: '22:00',
      });
      expect(shown(pickerInputs(element)[0])).toBe('8/21/2026');
    });

    it('opening the same event again puts its saved times back in the pickers', () => {
      const { editor, fixture, element } = create({ id: LEATHER_NIGHT });
      typeInto(fixture, pickerInputs(element)[0], '8/21/2026');
      fixture.componentRef.setInput('request', { id: LEATHER_NIGHT });
      fixture.detectChanges();
      expect(shown(pickerInputs(element)[0])).toBe('8/14/2026');
      expect(editor.form.controls.startDate.value).toBe('2026-08-14');
    });

    it('says the end is the next day only when it is', () => {
      const { editor, fixture, element } = create();
      const note = () => element.querySelector('.event-editor__end-note');
      editor.form.patchValue({ startDate: '2026-08-14', startTime: '19:00', endTime: '21:00' });
      fixture.detectChanges();
      expect(note()).toBeNull();
      expect(element.textContent).not.toContain('next');

      editor.form.patchValue({ startTime: '22:00', endTime: '02:00' });
      fixture.detectChanges();
      expect(text(note())).toBe('Ends the next day: Sat, Aug 15, 2:00 AM.');

      editor.form.patchValue({ endDate: '2026-08-15' });
      fixture.detectChanges();
      expect(note()).toBeNull();
    });
  });

  describe('editing', () => {
    it('opens a request in Seattle time, with the event’s own tags checked', () => {
      const { editor, element } = create({ id: LEATHER_NIGHT });
      expect(editor.editingId()).toBe(LEATHER_NIGHT);
      expect(editor.form.getRawValue()).toEqual({
        ...EMPTY_EVENT_FORM,
        title: 'Leather Night',
        description: 'Dress code encouraged, not enforced. Coat check at the door.',
        startDate: '2026-08-14',
        startTime: '21:00',
        endTime: '02:00',
        venueId: '1',
        costText: '$10 cover',
      });
      expect(editor.selectedCategoryIds()).toEqual([BAR]);
      expect(editor.selectedSceneTagIds()).toEqual([LEATHER]);
      const heading = element.querySelector('.event-editor__heading')!;
      expect(text(heading.querySelector('h3'))).toBe('Leather Night');
      expect(text(heading.querySelector('.event-editor__hint'))).toBe('Event #103');
      expect(heading.querySelector('a')?.getAttribute('href')).toBe('/events/103');
      expect(text(element.querySelector('button[type="submit"]'))).toBe('Save changes');
    });

    it('saves only what changed, and swaps the tags', async () => {
      const { crud, editor, saved } = create({ id: LEATHER_NIGHT });
      const update = vi.spyOn(crud, 'updateItem');
      editor.form.controls.costText.setValue('$15 cover');
      editor.toggleSceneTag(LEATHER, false);
      editor.toggleSceneTag(BEAR, true);
      editor.save();

      expect(update).toHaveBeenCalledTimes(1);
      expect(update.mock.calls[0][0]).toEqual({
        table_name: 'events',
        column_name: 'id',
        column_value: LEATHER_NIGHT,
        value: { cost_text: '$15 cover' },
      });
      expect((await eventRow(crud, LEATHER_NIGHT))['title']).toBe('Leather Night');
      expect(await categoriesOf(crud, LEATHER_NIGHT)).toEqual([BAR]);
      expect(await sceneTagsOf(crud, LEATHER_NIGHT)).toEqual([BEAR]);
      expect(saved).toEqual([{ id: LEATHER_NIGHT, added: false }]);
      expect(editor.editingId()).toBe(LEATHER_NIGHT);
    });

    it('empties a text field', async () => {
      const { crud, editor } = create({ id: LEATHER_NIGHT });
      const update = vi.spyOn(crud, 'updateItem');
      editor.form.controls.description.setValue('');
      editor.save();
      expect(update.mock.calls[0][0].value).toEqual({ description: '' });
      expect((await eventRow(crud, LEATHER_NIGHT))['description']).toBe('');
      expect(editor.formError()).toBe('');
    });

    it('shows only the problem when the event can’t be found, so it can’t be saved as new', () => {
      const { editor, element } = create({ id: '9999' });
      expect(editor.formError()).toBe("Event 9999 wasn't found.");
      expect(element.querySelector('form')).toBeNull();
      expect(text(element)).toBe("Event 9999 wasn't found.");
    });
  });

  describe('a new event', () => {
    it('starts blank', () => {
      const { editor, element } = create();
      expect(editor.editingId()).toBeNull();
      expect(editor.form.getRawValue()).toEqual(EMPTY_EVENT_FORM);
      expect(element.querySelector('.event-editor__heading')).toBeNull();
      expect(text(element.querySelector('button[type="submit"]'))).toBe('Add event');
    });

    it('is added as pending, with its tags, and the form is blank again', async () => {
      const { crud, editor, element, saved } = create();
      const add = vi.spyOn(crud, 'addItemFetchPrimaryKey');
      editor.form.setValue({
        ...EMPTY_EVENT_FORM,
        title: 'Elders Bingo',
        startDate: '2026-09-04',
        startTime: '19:00',
        endTime: '21:00',
        venueId: '3',
        costText: 'Free',
      });
      editor.toggleCategory(COMMUNITY, true);
      editor.toggleSceneTag(ELDERS, true);
      editor.save();

      const added = (await eventRows(crud)).find((row) => row['title'] === 'Elders Bingo')!;
      expect(added).toMatchObject({
        starts_at_us: String(pacificWallClockToUs(2026, 9, 4, 19)),
        ends_at_us: String(pacificWallClockToUs(2026, 9, 4, 21)),
        venue_id: '3',
        cost_text: 'Free',
        status: 'pending',
        created_by_person_id: '1',
      });
      // A blank number column is left out of the insert ('' isn't a number).
      expect(add.mock.calls[0][0].value).not.toHaveProperty('series_id');
      expect(await categoriesOf(crud, added['id'])).toEqual([COMMUNITY]);
      expect(await sceneTagsOf(crud, added['id'])).toEqual([ELDERS]);
      expect(saved).toEqual([{ id: added['id'], added: true }]);
      // Ready for the next one, pickers included.
      expect(editor.editingId()).toBeNull();
      expect(editor.form.getRawValue()).toEqual(EMPTY_EVENT_FORM);
      expect(editor.selectedCategoryIds()).toEqual([]);
      expect(pickerInputs(element).map(shown)).toEqual(['', '', '', '']);
    });

    it('writes no tag rows for a series event left unchecked, so it keeps the series’', async () => {
      const { crud, editor } = create();
      editor.form.setValue({
        ...EMPTY_EVENT_FORM,
        title: 'Bear Happy Hour',
        startDate: '2026-09-10',
        startTime: '18:00',
        venueId: '1',
        seriesId: '11',
      });
      editor.save();
      const added = (await eventRows(crud)).find(
        (row) => row['title'] === 'Bear Happy Hour' && row['status'] === 'pending',
      )!;
      expect(added['series_id']).toBe('11');
      expect(await categoriesOf(crud, added['id'])).toEqual([]);
      expect(await sceneTagsOf(crud, added['id'])).toEqual([]);
    });

    it('asks for a start before saving', () => {
      const { crud, editor, saved } = create();
      const add = vi.spyOn(crud, 'addItemFetchPrimaryKey');
      editor.form.controls.title.setValue('No date yet');
      editor.save();
      expect(editor.formError()).toBe('Fill in the title and the start date and time.');
      expect(add).not.toHaveBeenCalled();
      expect(saved).toEqual([]);
    });

    it('keeps the cost hint from running into the field below', () => {
      const { element } = create();
      const cost = Array.from(element.querySelectorAll('mat-form-field')).find((field) =>
        text(field.querySelector('mat-label')).startsWith('Cost'),
      )!;
      expect(cost.querySelector('.mat-mdc-form-field-subscript-dynamic-size')).not.toBeNull();
    });
  });
});
