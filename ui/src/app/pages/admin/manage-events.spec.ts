import { TestBed } from '@angular/core/testing';
import { By } from '@angular/platform-browser';
import { provideRouter } from '@angular/router';
import { RouterTestingHarness } from '@angular/router/testing';
import { provideNoopAnimations } from '@angular/platform-browser/animations';
import { MatAutocompleteSelectedEvent } from '@angular/material/autocomplete';
import { firstValueFrom } from 'rxjs';
import { vi } from 'vitest';
import { CrudAccess } from '@honuware/ui/access';
import { AuthService } from '@honuware/ui/auth';
import { ToastService } from '@honuware/ui/foundation';
import { provideHonuwareAccessMock } from '@honuware/ui/testing';
import { provideCommunityAccess } from '../../core/community-access/community-access.providers';
import { EVENTS_ADMIN_ACCESS } from '../../core/events-admin-access/events-admin-access';
import { provideEventsAdminAccess } from '../../core/events-admin-access/events-admin-access.providers';
import { HONUWARE_MOCK_OPTIONS } from '../../core/mock/honuware-mock';
import { SubmissionSettingsMockStore } from '../../core/mock/submission-settings-mock-store';
import { dataResultsToRecords } from '../../core/types/events.types';
import { pacificWallClockToUs } from '../../core/utils/events-date.utils';
import { EventEditor } from './event-editor';
import { EMPTY_EVENT_FORM } from './manage-events-form';
import {
  EDIT_EVENT_TAB,
  ManageEventsPage,
  NEW_EVENT_TAB,
  PendingEvent,
  QueueGroup,
} from './manage-events';

// Wed Aug 12, 2026, 3:00 PM Pacific: the mock dataset's reference week.
const NOW_US = pacificWallClockToUs(2026, 8, 12, 15);

// In the seed: Open Mic is the one pending event a curator entered; Leather
// Night is a listed one.
const OPEN_MIC = '117';
const LEATHER_NIGHT = '103';

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

// The seed's two sample submissions (EV3 §5.5): #1 is Ada's two-date Queer
// Craft Night with no link; #2 is Grace's "Leather Night!", an hour after the
// listed Leather Night at the same venue.
const CRAFT_NIGHT_FIRST = 201;
const CRAFT_NIGHT_SECOND = 202;
const LEATHER_NIGHT_AGAIN = 203;

// The offline wiring the `local` build uses: the generic CRUD store seeded
// with the events tables, the review mock on top of it, and the vocabularies.
function configure() {
  let crud!: CrudAccess;
  TestBed.configureTestingModule({
    providers: [
      provideRouter([{ path: 'admin/events', component: ManageEventsPage }]),
      provideNoopAnimations(),
      provideHonuwareAccessMock(HONUWARE_MOCK_OPTIONS, (mocks) => (crud = mocks.crud)),
      provideCommunityAccess(true),
      provideEventsAdminAccess(true),
      { provide: AuthService, useValue: { authData: ADMIN } },
    ],
  });
  return crud;
}

// `before` runs once the providers are set and before the page opens.
async function open(url = '/admin/events', before: () => void = () => undefined) {
  const crud = configure();
  before();
  const harness = await RouterTestingHarness.create();
  const page = await harness.navigateByUrl(url, ManageEventsPage);
  return { crud, harness, page, element: harness.routeNativeElement as HTMLElement };
}

async function eventRow(crud: CrudAccess, id: string) {
  return dataResultsToRecords(await firstValueFrom(crud.getRow('events', 'id', id)))[0];
}

async function submissionRow(crud: CrudAccess, id: string) {
  return dataResultsToRecords(await firstValueFrom(crud.getRow('event_submissions', 'id', id)))[0];
}

function queuedEvent(page: ManageEventsPage, id: number): PendingEvent {
  return page.pending().find((item) => item.id === id)!;
}

function submissionGroup(page: ManageEventsPage, submissionId: number): QueueGroup {
  return page.queue().find((group) => group.submission?.id === submissionId)!;
}

function text(element: Element | null | undefined): string {
  return (element?.textContent ?? '').replace(/\s+/g, ' ').trim();
}

function buttonLabels(element: Element): string[] {
  return Array.from(element.querySelectorAll('button')).map(text);
}

// The card's own top-line buttons (not the ones in its details).
function headerButtons(card: Element): string[] {
  return buttonLabels(card.querySelector('.manage-events__entry-header')!);
}

// The open details' facts, as [label, value] pairs.
function facts(card: Element): string[][] {
  const cells = Array.from(card.querySelectorAll('.manage-events__facts > *')).map(text);
  return cells.flatMap((cell, index) => (index % 2 === 0 ? [[cell, cells[index + 1]]] : []));
}

function detailsToggle(card: Element): HTMLButtonElement {
  return card.querySelector('.manage-events__toggle') as HTMLButtonElement;
}

// The editors on the page, and which tab each is in (by whether it was asked
// to open an event).
function editors(harness: RouterTestingHarness): EventEditor[] {
  return harness
    .routeDebugElement!.queryAll(By.directive(EventEditor))
    .map((editor) => editor.componentInstance as EventEditor);
}

function newEditor(harness: RouterTestingHarness): EventEditor | undefined {
  return editors(harness).find((editor) => editor.request() === null);
}

function editEditor(harness: RouterTestingHarness): EventEditor | undefined {
  return editors(harness).find((editor) => editor.request() !== null);
}

function tabLabels(element: HTMLElement): string[] {
  return Array.from(element.querySelectorAll('[role="tab"]')).map(text);
}

function selectedTabLabel(element: HTMLElement): string {
  return text(element.querySelector('[role="tab"][aria-selected="true"]'));
}

describe('ManageEventsPage', () => {
  beforeEach(() => {
    vi.useFakeTimers({ toFake: ['Date'] });
    vi.setSystemTime(new Date(NOW_US / 1000));
  });
  afterEach(() => {
    vi.useRealTimers();
    vi.restoreAllMocks();
  });

  describe('the review queue', () => {
    it('lists what is waiting, with when and where', async () => {
      const { page, element } = await open();
      expect(page.pending()).toEqual([
        {
          id: 117,
          title: 'Open Mic (awaiting review)',
          when: 'Thu, Aug 13 · 8:00 PM – 10:00 PM',
          where: 'Driftwood Hall',
          description: '',
          url: '',
          costText: '',
          submissionId: null,
          possibleDuplicates: [],
          noLink: false,
        },
        {
          id: LEATHER_NIGHT_AGAIN,
          title: 'Leather Night!',
          when: 'Fri, Aug 14 · 10:00 PM – 12:00 AM',
          where: 'The Lantern Room',
          description: 'Gear night. Coat check at the door.',
          url: 'https://example.org/leather-night',
          costText: '$10 cover',
          submissionId: 2,
          possibleDuplicates: [{ id: 103, title: 'Leather Night' }],
          noLink: false,
        },
        {
          id: CRAFT_NIGHT_FIRST,
          title: 'Queer Craft Night',
          when: 'Tue, Aug 18 · 7:00 PM – 9:00 PM',
          where: 'Columbia City Social Club',
          description: 'Bring a project, or start one with the yarn and fabric we bring.',
          url: '',
          costText: 'Free',
          submissionId: 1,
          possibleDuplicates: [],
          noLink: true,
        },
        {
          id: CRAFT_NIGHT_SECOND,
          title: 'Queer Craft Night',
          when: 'Tue, Sep 1 · 7:00 PM – 9:00 PM',
          where: 'Columbia City Social Club',
          description: 'Bring a project, or start one with the yarn and fabric we bring.',
          url: '',
          costText: 'Free',
          submissionId: 1,
          possibleDuplicates: [],
          noLink: true,
        },
      ]);
      expect(element.textContent).toContain('Waiting for review (4)');
    });

    it('approves an event and takes it off the queue', async () => {
      const { crud, harness, page } = await open();
      page.approve(queuedEvent(page, Number(OPEN_MIC)));
      harness.detectChanges();
      expect((await eventRow(crud, OPEN_MIC))['status']).toBe('approved');
      expect(page.pending().map((item) => item.id)).not.toContain(Number(OPEN_MIC));
    });

    it('says so when nothing is waiting', async () => {
      const { harness, page, element } = await open();
      for (const group of page.queue()) {
        if (group.submission) {
          page.approveAll(group);
        } else {
          page.approve(group.events[0]);
        }
      }
      harness.detectChanges();
      expect(page.pending()).toEqual([]);
      expect(element.textContent).toContain('Nothing is waiting for review.');
    });

    it('rejects in the card with a note, and does nothing when cancelled', async () => {
      const { crud, harness, page, element } = await open();
      const openMic = queuedEvent(page, Number(OPEN_MIC));
      page.reject(openMic);
      harness.detectChanges();
      expect(page.isRejecting('e117')).toBe(true);
      expect(element.querySelector('[data-event="117"] .manage-events__reject')).not.toBeNull();
      page.cancelReject();
      harness.detectChanges();
      expect(element.querySelector('.manage-events__reject')).toBeNull();
      expect((await eventRow(crud, OPEN_MIC))['status']).toBe('pending');

      page.reject(openMic);
      page.rejectForm.controls.notes.setValue('Duplicate of the Friday listing');
      page.confirmReject();
      const row = await eventRow(crud, OPEN_MIC);
      expect(row['status']).toBe('rejected');
      expect(row['review_notes']).toBe('Duplicate of the Friday listing');
      expect(page.pending().map((item) => item.id)).not.toContain(Number(OPEN_MIC));
      expect(page.rejecting()).toBeNull();
    });
  });

  describe('the queue cards', () => {
    it('shows each one as a raised card: the title first, then the date, then the buttons', async () => {
      const { element } = await open();
      const cards = Array.from(element.querySelectorAll('.manage-events__entry'));
      expect(cards.length).toBe(3);
      const openMic = element.querySelector('[data-event="117"]')!;
      expect(openMic.classList).toContain('manage-events__entry');
      const lines = Array.from(openMic.querySelector('.manage-events__entry-text')!.children);
      expect(lines[0].tagName).toBe('H3');
      expect(text(lines[0])).toBe('Open Mic (awaiting review)');
      expect(text(lines[1])).toBe('Thu, Aug 13 · 8:00 PM – 10:00 PM');
      expect(headerButtons(openMic)).toEqual(['Approve', 'Reject', 'Details expand_more']);
    });

    it('sums up a submission’s dates, with buttons for all of them', async () => {
      const { element } = await open();
      const craftNight = element.querySelector('[data-submission="1"]')!;
      expect(text(craftNight.querySelector('.manage-events__entry-title'))).toBe(
        'Queer Craft Night',
      );
      expect(text(craftNight.querySelector('.manage-events__entry-when'))).toBe(
        'Tue, Aug 18 · 7:00 PM – 9:00 PM, and 1 more date',
      );
      expect(headerButtons(craftNight)).toEqual([
        'Approve all',
        'Reject all',
        'Details expand_more',
      ]);
      // The flags stay on the top line, for deciding without opening it.
      expect(text(craftNight.querySelector('.manage-events__entry-header'))).toContain('No link');
      const leatherNight = element.querySelector('[data-submission="2"]')!;
      expect(leatherNight.getAttribute('data-event')).toBe('203');
      expect(text(leatherNight.querySelector('.manage-events__entry-header'))).toContain(
        'Possible duplicate of #103 Leather Night',
      );
      // One date needs no "all" buttons.
      expect(headerButtons(leatherNight)).toEqual(['Approve', 'Reject', 'Details expand_more']);
    });

    it('keeps the details closed until asked', async () => {
      const { harness, element } = await open();
      expect(element.querySelector('.manage-events__details')).toBeNull();
      expect(element.textContent).not.toContain('Submitted by');

      const leatherNight = element.querySelector('[data-submission="2"]')!;
      const toggle = detailsToggle(leatherNight);
      expect(toggle.getAttribute('aria-expanded')).toBe('false');
      toggle.click();
      harness.detectChanges();
      expect(toggle.getAttribute('aria-expanded')).toBe('true');
      const details = leatherNight.querySelector('.manage-events__details')!;
      expect(details.id).toBe(toggle.getAttribute('aria-controls'));
      expect(facts(details)).toEqual([
        ['Where', 'The Lantern Room'],
        ['Cost', '$10 cover'],
        ['Link', 'https://example.org/leather-night'],
        ['Description', 'Gear night. Coat check at the door.'],
      ]);
      const link = details.querySelector('.manage-events__facts a')!;
      expect(link.getAttribute('href')).toBe('https://example.org/leather-night');
      expect(link.getAttribute('rel')).toBe('noopener noreferrer');
      // The other cards stay closed.
      expect(element.querySelectorAll('.manage-events__details').length).toBe(1);

      toggle.click();
      harness.detectChanges();
      expect(leatherNight.querySelector('.manage-events__details')).toBeNull();
    });

    it('shows who sent it and their note in the details', async () => {
      const { harness, element } = await open();
      const craftNight = element.querySelector('[data-submission="1"]')!;
      detailsToggle(craftNight).click();
      harness.detectChanges();
      expect(text(craftNight.querySelector('.manage-events__badge'))).toBe(
        'Submitted by Ada Lovelace',
      );
      expect(craftNight.textContent).toContain("I'm the organizer");
      expect(craftNight.textContent).toContain('A regular thing');
      expect(text(craftNight.querySelector('.manage-events__note'))).toBe(
        'We meet every other week at the back tables.',
      );
    });

    it('lists a submission’s dates in the details, each with its own buttons', async () => {
      const { harness, page, element } = await open();
      const craftNight = element.querySelector('[data-submission="1"]')!;
      detailsToggle(craftNight).click();
      harness.detectChanges();
      const dates = Array.from(craftNight.querySelectorAll('.manage-events__date'));
      expect(dates.map((date) => date.getAttribute('data-event'))).toEqual(['201', '202']);
      const secondDate = dates[1].querySelector('.manage-events__date-text')!;
      expect(text(secondDate.firstElementChild)).toBe('Tue, Sep 1 · 7:00 PM – 9:00 PM');
      expect(text(secondDate.querySelector('.manage-events__flags'))).toBe('No link');
      expect(buttonLabels(dates[0])).toEqual(['Approve', 'Reject', 'Edit']);
      expect(dates[0].querySelector('a')?.getAttribute('href')).toContain('/events/201');

      page.reject(queuedEvent(page, CRAFT_NIGHT_SECOND));
      harness.detectChanges();
      expect(dates[1].querySelector('.manage-events__reject')).not.toBeNull();
    });

    it('shows an event a curator entered without the submission parts', async () => {
      const { harness, page, element } = await open();
      const openMic = element.querySelector('[data-event="117"]')!;
      expect(openMic.querySelector('.manage-events__flags')).toBeNull();
      detailsToggle(openMic).click();
      harness.detectChanges();
      expect(openMic.querySelector('.manage-events__badge')).toBeNull();
      expect(facts(openMic)).toEqual([['Where', 'Driftwood Hall']]);

      // Its reject panel asks only for the curator note.
      page.reject(queuedEvent(page, Number(OPEN_MIC)));
      harness.detectChanges();
      expect(page.rejectingSubmitted()).toBe(false);
      const panel = openMic.querySelector('.manage-events__reject')!;
      expect(panel.querySelector('mat-select')).toBeNull();
      expect(panel.querySelector('textarea')).toBeNull();
      expect(panel.querySelector('input')).not.toBeNull();
    });

    it('opens the event in Edit Event from the card’s Edit', async () => {
      const { harness, page, element } = await open();
      const openMic = element.querySelector('[data-event="117"]')!;
      detailsToggle(openMic).click();
      harness.detectChanges();
      const edit = Array.from(openMic.querySelectorAll('button')).find(
        (button) => text(button) === 'edit Edit',
      )!;
      edit.click();
      harness.detectChanges();
      expect(page.selectedTab()).toBe(EDIT_EVENT_TAB);
      expect(selectedTabLabel(element)).toBe('Edit Event');
      expect(editEditor(harness)?.editingId()).toBe(OPEN_MIC);
    });
  });

  describe('submitted events (EV3 §5.5)', () => {
    it('groups a submission’s dates under who sent it, in start order', async () => {
      const { page } = await open();
      expect(page.queue().map((group) => group.key)).toEqual(['e117', 's2', 's1']);
      expect(submissionGroup(page, 1)).toEqual({
        key: 's1',
        submission: {
          id: 1,
          submitter: 'Ada Lovelace',
          relationship: "I'm the organizer",
          note: 'We meet every other week at the back tables.',
          regularThing: true,
        },
        events: [queuedEvent(page, CRAFT_NIGHT_FIRST), queuedEvent(page, CRAFT_NIGHT_SECOND)],
        possibleDuplicates: [],
        noLink: true,
      });
      expect(submissionGroup(page, 2)).toMatchObject({
        submission: {
          id: 2,
          submitter: 'Grace Hopper',
          relationship: "I'm going",
          note: '',
          regularThing: false,
        },
        possibleDuplicates: [{ id: 103, title: 'Leather Night' }],
        noLink: false,
      });
    });

    it('approves all of a submission’s dates with one call', async () => {
      const { crud, page } = await open();
      const access = TestBed.inject(EVENTS_ADMIN_ACCESS);
      const reviewSubmission = vi.spyOn(access, 'reviewSubmission');
      const reviewEvent = vi.spyOn(access, 'reviewEvent');
      const success = vi.spyOn(TestBed.inject(ToastService), 'success');
      page.approveAll(submissionGroup(page, 1));

      expect(reviewSubmission).toHaveBeenCalledTimes(1);
      expect(reviewSubmission).toHaveBeenCalledWith(1, 'approve', { notes: '' });
      expect(reviewEvent).not.toHaveBeenCalled();
      expect((await eventRow(crud, String(CRAFT_NIGHT_FIRST)))['status']).toBe('approved');
      expect((await eventRow(crud, String(CRAFT_NIGHT_SECOND)))['status']).toBe('approved');
      expect(page.queue().map((group) => group.key)).toEqual(['e117', 's2']);
      expect(success).toHaveBeenCalledWith(
        'Approved all 2 dates of "Queer Craft Night". The submitter has been emailed.',
      );
    });

    it('sends the reason, the curator note and the message from the reject panel', async () => {
      const { crud, harness, page, element } = await open();
      const reviewSubmission = vi.spyOn(TestBed.inject(EVENTS_ADMIN_ACCESS), 'reviewSubmission');
      page.rejectAll(submissionGroup(page, 1));
      harness.detectChanges();
      const panel = element.querySelector('[data-submission="1"] .manage-events__reject')!;
      expect(panel.querySelector('mat-select')).not.toBeNull();
      expect(panel.querySelector('textarea')).not.toBeNull();
      expect(page.rejectReasons.map((reason) => reason.label)).toEqual([
        'Already listed',
        'Not a fit',
        'Missing details',
        'Already over',
        'Spam',
        'Other',
      ]);

      page.rejectForm.setValue({
        reason: 'missing-details',
        notes: 'No address for the back room',
        message: 'Could you add which room it’s in and send it again?',
      });
      page.confirmReject();

      expect(reviewSubmission).toHaveBeenCalledTimes(1);
      expect(reviewSubmission).toHaveBeenCalledWith(1, 'reject', {
        notes: 'No address for the back room',
        reason: 'missing-details',
        message_to_submitter: 'Could you add which room it’s in and send it again?',
      });
      const event = await eventRow(crud, String(CRAFT_NIGHT_FIRST));
      expect(event['status']).toBe('rejected');
      expect(event['review_notes']).toBe('No address for the back room');
      expect(await submissionRow(crud, '1')).toMatchObject({
        reject_reason: 'missing-details',
        message_to_submitter: 'Could you add which room it’s in and send it again?',
      });
    });

    it('leaves the reason and message out when they are blank', async () => {
      const { page } = await open();
      const reviewEvent = vi.spyOn(TestBed.inject(EVENTS_ADMIN_ACCESS), 'reviewEvent');
      page.reject(queuedEvent(page, LEATHER_NIGHT_AGAIN));
      expect(page.rejectingSubmitted()).toBe(true);
      page.rejectForm.controls.notes.setValue('  Same night as #103  ');
      page.confirmReject();
      expect(reviewEvent).toHaveBeenCalledWith(LEATHER_NIGHT_AGAIN, 'reject', {
        notes: 'Same night as #103',
      });
    });

    it('says in the toast when the submitter was emailed', async () => {
      const { page } = await open();
      const success = vi.spyOn(TestBed.inject(ToastService), 'success');
      // The first of two dates: the submission isn't decided yet.
      page.approve(queuedEvent(page, CRAFT_NIGHT_FIRST));
      expect(success).toHaveBeenLastCalledWith('Approved "Queer Craft Night".');
      page.approve(queuedEvent(page, CRAFT_NIGHT_SECOND));
      expect(success).toHaveBeenLastCalledWith(
        'Approved "Queer Craft Night". The submitter has been emailed.',
      );
    });

    it('shows the paused banner only while submissions are paused', async () => {
      const open1 = await open();
      expect(open1.element.querySelector('.manage-events__paused')).toBeNull();
      TestBed.resetTestingModule();

      const open2 = await open('/admin/events', () =>
        TestBed.inject(SubmissionSettingsMockStore).save({ submissions_paused: true }),
      );
      const banner = open2.element.querySelector('.manage-events__paused');
      expect(banner?.textContent).toContain(
        'Submissions are paused. Nobody can submit an event right now.',
      );
      expect(banner?.querySelector('a')?.getAttribute('href')).toBe('/admin/events/settings');
    });

    it('links to the submission limits page', async () => {
      const { element } = await open();
      const link = element.querySelector('.manage-events__header a');
      expect(link?.textContent).toContain('Submission limits');
      expect(link?.getAttribute('href')).toBe('/admin/events/settings');
    });
  });

  describe('the New Event and Edit Event tabs', () => {
    it('opens on New Event, with a blank form', async () => {
      const { harness, page, element } = await open();
      expect(tabLabels(element)).toEqual(['New Event', 'Edit Event']);
      expect(page.selectedTab()).toBe(NEW_EVENT_TAB);
      expect(selectedTabLabel(element)).toBe('New Event');
      expect(newEditor(harness)?.form.getRawValue()).toEqual(EMPTY_EVENT_FORM);
      expect(editEditor(harness)).toBeUndefined();
    });

    it('says how to pick an event on Edit Event', async () => {
      const { harness, page, element } = await open();
      page.selectedTab.set(EDIT_EVENT_TAB);
      harness.detectChanges();
      const tab = element.querySelector('.mat-mdc-tab-body-active')!;
      expect(text(tab.querySelector('mat-label'))).toBe('Find an event to edit');
      expect(text(tab)).toContain(
        'Search for an event by its title, or press Edit on one in the queue.',
      );
      expect(editEditor(harness)).toBeUndefined();
    });

    it('opens ?edit=<id> in Edit Event', async () => {
      const { harness, page, element } = await open(`/admin/events?edit=${LEATHER_NIGHT}`);
      expect(page.selectedTab()).toBe(EDIT_EVENT_TAB);
      expect(selectedTabLabel(element)).toBe('Edit Event');
      const editor = editEditor(harness)!;
      expect(editor.editingId()).toBe(LEATHER_NIGHT);
      expect(editor.form.controls.title.value).toBe('Leather Night');
    });

    it('opens an event found by title in Edit Event, and clears the search', async () => {
      const { harness, page } = await open();
      page.searchControl.setValue('Leather');
      page.onSearchSelected({
        option: { value: { value: LEATHER_NIGHT, display: 'Leather Night' } },
      } as MatAutocompleteSelectedEvent);
      harness.detectChanges();
      expect(page.selectedTab()).toBe(EDIT_EVENT_TAB);
      expect(editEditor(harness)?.editingId()).toBe(LEATHER_NIGHT);
      expect(page.searchControl.value).toBe('');
    });

    it('keeps what was typed in New Event while Edit Event is open', async () => {
      const { harness, page } = await open();
      newEditor(harness)!.form.controls.title.setValue('Half-typed');
      page.editEvent(LEATHER_NIGHT);
      harness.detectChanges();
      page.selectedTab.set(NEW_EVENT_TAB);
      harness.detectChanges();
      expect(newEditor(harness)?.form.controls.title.value).toBe('Half-typed');
      expect(editEditor(harness)?.form.controls.title.value).toBe('Leather Night');
    });

    it('opens a newly added event in Edit Event, puts it in the queue, and blanks New Event', async () => {
      const { harness, page, element } = await open();
      const editor = newEditor(harness)!;
      editor.form.setValue({
        ...EMPTY_EVENT_FORM,
        title: 'Elders Bingo',
        startDate: '2026-09-04',
        startTime: '19:00',
        venueId: '3',
      });
      editor.save();
      harness.detectChanges();

      const added = page.pending().find((item) => item.title === 'Elders Bingo')!;
      expect(added).toBeDefined();
      expect(page.selectedTab()).toBe(EDIT_EVENT_TAB);
      expect(selectedTabLabel(element)).toBe('Edit Event');
      expect(editEditor(harness)?.editingId()).toBe(String(added.id));
      expect(newEditor(harness)?.form.getRawValue()).toEqual(EMPTY_EVENT_FORM);
    });
  });
});
