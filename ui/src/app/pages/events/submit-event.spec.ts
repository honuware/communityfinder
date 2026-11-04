import { TestBed } from '@angular/core/testing';
import { By } from '@angular/platform-browser';
import { provideRouter } from '@angular/router';
import { RouterTestingHarness } from '@angular/router/testing';
import { provideNoopAnimations } from '@angular/platform-browser/animations';
import { Observable, of, throwError } from 'rxjs';
import { vi } from 'vitest';
import { HONUWARE_API_BASE } from '@honuware/ui/access';
import { AuthService } from '@honuware/ui/auth';
import { SimpleDateComponent } from '@honuware/ui/controls';
import { provideHonuwareAccessMock } from '@honuware/ui/testing';
import { provideCommunityAccess } from '../../core/community-access/community-access.providers';
import {
  EVENT_SUBMISSIONS_ACCESS,
  SubmissionProblem,
  SubmissionRequest,
  SubmissionResult,
} from '../../core/event-submissions-access/event-submissions-access';
import { pacificWallClockToUs } from '../../core/utils/events-date.utils';
import { MAX_SUBMISSION_DATES } from './submit-event-form';
import { SubmitEventPage } from './submit-event';
import routes from './events.routes';

const MEMBER = { isAuth: true, personId: 2, firstName: 'Ada' };
const SIGNED_OUT = { isAuth: false };

const LANTERN_ROOM = { id: 1, name: 'The Lantern Room', address: '' };

const RESULT: SubmissionResult = {
  submission_id: 7,
  event_ids: [41, 42],
  ignored_category_slugs: [],
  ignored_scene_tag_slugs: [],
  possible_duplicates: [
    {
      date_index: 1,
      event_id: 103,
      title: 'Leather Night',
      starts_at_us: pacificWallClockToUs(2026, 8, 14, 21),
    },
  ],
};

function refusal(fields: Partial<SubmissionProblem>): SubmissionProblem {
  return { status: 400, error_code: '', field: '', date_index: null, detail: 'No', ...fields };
}

interface Options {
  authData?: object;
  accepting?: boolean[];
  submit?: (request: SubmissionRequest) => Observable<SubmissionResult>;
}

// The submissions access is a stub: getStatus answers from `accepting` in
// turn (the last answer repeats), and submit as given.
function configure({ authData = MEMBER, accepting = [true], submit }: Options = {}) {
  const answers = [...accepting];
  const access = {
    getStatus: vi.fn(() => of(answers.length > 1 ? answers.shift()! : answers[0])),
    submit: vi.fn(submit ?? (() => of(RESULT))),
    getMine: vi.fn(() => of([])),
    withdraw: vi.fn(),
  };
  TestBed.configureTestingModule({
    providers: [
      provideRouter([{ path: 'events', children: routes }]),
      provideNoopAnimations(),
      provideHonuwareAccessMock({}),
      provideCommunityAccess(true),
      { provide: HONUWARE_API_BASE, useValue: '/api' },
      { provide: AuthService, useValue: { authData$: of(authData) } },
      { provide: EVENT_SUBMISSIONS_ACCESS, useValue: access },
    ],
  });
  return access;
}

async function open(options: Options = {}) {
  const access = configure(options);
  const harness = await RouterTestingHarness.create();
  const page = await harness.navigateByUrl('/events/submit', SubmitEventPage);
  return { access, harness, page, element: harness.routeNativeElement as HTMLElement };
}

// The fewest fields that send: a title, the fall-back night, a venue, the
// agreement.
function fillIn(page: SubmitEventPage) {
  page.form.patchValue({ title: 'Leather Night', agreed: true });
  page.dateRows
    .at(0)
    .setValue({ date: '2026-10-31', startTime: '22:00', endDate: '', endTime: '03:00' });
  page.onVenueSelected({ option: { value: LANTERN_ROOM } } as never);
}

function text(element: Element | null | undefined): string {
  return element?.textContent?.replace(/\s+/g, ' ').trim() ?? '';
}

// A date row's picker boxes: start date, start time, end time, end date.
function pickerInputs(element: HTMLElement, row = 0): HTMLInputElement[] {
  return Array.from(
    element.querySelectorAll('.submit-event__date')[row].querySelectorAll('hw-simple-date input'),
  );
}

// What a picker box shows, with the time's narrow space before AM/PM made plain.
function shown(input: HTMLInputElement): string {
  return input.value.replace(/\s+/g, ' ');
}

function typeInto(harness: RouterTestingHarness, input: HTMLInputElement, value: string) {
  input.value = value;
  input.dispatchEvent(new Event('input', { bubbles: true }));
  harness.detectChanges();
}

describe('SubmitEventPage', () => {
  afterEach(() => vi.restoreAllMocks());

  describe('before the form', () => {
    it('explains and offers sign-in and registration when signed out', async () => {
      const { element } = await open({ authData: SIGNED_OUT });
      const notice = element.querySelector('.submit-event__signed-out')!;
      expect(text(notice)).toContain('To submit an event, sign in or create an account.');
      const links = Array.from(notice.querySelectorAll('a')).map((link) => [
        text(link),
        link.getAttribute('href'),
      ]);
      expect(links).toEqual([
        ['Sign in', '/login?returnUrl=%2Fevents%2Fsubmit'],
        ['Create an account', '/register'],
      ]);
      expect(element.querySelector('form')).toBeNull();
    });

    it('says the form is the only way in, signed in or not', async () => {
      const { element } = await open({ authData: SIGNED_OUT });
      expect(text(element.querySelector('.submit-event__intake'))).toContain(
        'This form is the only way we take event listings.',
      );
    });

    it('shows the paused notice on load, and the form once they are back on', async () => {
      const { access, harness, element } = await open({ accepting: [false, true] });
      expect(text(element.querySelector('.submit-event__paused'))).toContain(
        "We're not taking event submissions right now. Please check back soon.",
      );
      expect(element.querySelector('form')).toBeNull();

      Array.from(element.querySelectorAll('button'))
        .find((button) => text(button) === 'Check again')!
        .click();
      harness.detectChanges();
      expect(access.getStatus).toHaveBeenCalledTimes(2);
      expect(element.querySelector('.submit-event__paused')).toBeNull();
      expect(element.querySelector('form')).not.toBeNull();
    });

    it('shows the paused notice to a signed-out visitor too', async () => {
      const { element } = await open({ authData: SIGNED_OUT, accepting: [false] });
      expect(element.querySelector('.submit-event__paused')).not.toBeNull();
      expect(element.querySelector('.submit-event__signed-out')).toBeNull();
    });

    it('sets the page title', async () => {
      await open();
      expect(document.title).toContain('Submit an event');
    });

    it('asks how they know about it, the organizer first', async () => {
      const { page } = await open();
      expect(page.relationships.map((entry) => entry.label)).toEqual([
        "I'm the organizer",
        'I work at the venue',
        "I'm performing",
        "I'm going",
        'Something else',
      ]);
      expect(page.form.controls.relationship.value).toBe('organizer');
    });
  });

  describe('the form', () => {
    it('sends Seattle wall-clock dates as instants, across the fall-back night', async () => {
      const { access, page } = await open();
      fillIn(page);
      page.addDate();
      page.dateRows
        .at(1)
        .setValue({ date: '2026-03-07', startTime: '22:00', endDate: '', endTime: '03:00' });
      page.send();
      expect(access.submit).toHaveBeenCalledTimes(1);
      expect(access.submit.mock.calls[0][0]).toEqual({
        title: 'Leather Night',
        dates: [
          {
            starts_at_us: Date.UTC(2026, 10, 1, 5) * 1000,
            ends_at_us: Date.UTC(2026, 10, 1, 11) * 1000,
          },
          {
            starts_at_us: Date.UTC(2026, 2, 8, 6) * 1000,
            ends_at_us: Date.UTC(2026, 2, 8, 10) * 1000,
          },
        ],
        venue_id: 1,
        relationship: 'organizer',
        regular_thing: false,
        agreed: true,
      });
    });

    it(`takes up to ${MAX_SUBMISSION_DATES} dates`, async () => {
      const { harness, page, element } = await open();
      expect(text(element.querySelector('.submit-event__group .submit-event__hint'))).toContain(
        'Up to 12 dates.',
      );
      for (let added = 0; added < MAX_SUBMISSION_DATES + 2; added++) {
        page.addDate();
      }
      harness.detectChanges();
      expect(page.dateRows.length).toBe(MAX_SUBMISSION_DATES);
      expect(element.querySelectorAll('.submit-event__date').length).toBe(MAX_SUBMISSION_DATES);
      const add = Array.from(element.querySelectorAll('button')).find((button) =>
        text(button).includes('Add another date'),
      )!;
      expect(add.disabled).toBe(true);

      page.removeDate(0);
      harness.detectChanges();
      expect(add.disabled).toBe(false);
    });

    it('caps each kind of tag at 3', async () => {
      const { harness, page, element } = await open();
      const chips = () =>
        Array.from(
          element.querySelectorAll('mat-chip-listbox[aria-label="Categories"] mat-chip-option'),
        );
      expect(chips().length).toBeGreaterThan(3);
      expect(chips().filter((chip) => chip.classList.contains('mat-mdc-chip-disabled'))).toEqual(
        [],
      );

      for (const slug of ['bar', 'drag', 'community']) {
        page.toggleCategory(slug, true);
      }
      harness.detectChanges();
      const disabled = chips().filter((chip) => chip.classList.contains('mat-mdc-chip-disabled'));
      expect(disabled.length).toBe(chips().length - 3);

      page.toggleCategory('drag', false);
      harness.detectChanges();
      expect(chips().some((chip) => chip.classList.contains('mat-mdc-chip-disabled'))).toBe(false);
    });

    it('hides the honeypot from people and sends it only when filled in', async () => {
      const { access, page, element } = await open();
      const honeypot = element.querySelector('.submit-event__website')!;
      expect(honeypot.getAttribute('aria-hidden')).toBe('true');
      const input = honeypot.querySelector('input')!;
      expect(input.getAttribute('tabindex')).toBe('-1');
      expect(input.getAttribute('autocomplete')).toBe('off');
      expect(getComputedStyle(honeypot).position).toBe('absolute');

      fillIn(page);
      page.send();
      expect('website' in access.submit.mock.calls[0][0]).toBe(false);
    });

    it('picks dates and times with the honuware pickers, start then end', async () => {
      const { harness, element } = await open();
      const row = element.querySelector('.submit-event__date')!;
      const pickers = harness
        .routeDebugElement!.queryAll(By.directive(SimpleDateComponent))
        .map((picker) => picker.componentInstance as SimpleDateComponent);
      expect(pickers.map((picker) => picker.type)).toEqual(['date', 'time', 'time', 'date']);
      expect(pickers[1].timeIncrementMinutes).toBe(15);
      const fields = Array.from(row.querySelectorAll('hw-simple-date mat-form-field'));
      expect(fields.map((field) => text(field.querySelector('mat-label')))).toEqual([
        'Start date',
        'Start time',
        'End time',
        'End date',
      ]);
      expect(row.querySelectorAll('mat-datepicker-toggle').length).toBe(2);
      expect(row.querySelectorAll('mat-timepicker-toggle').length).toBe(2);
      for (const field of fields) {
        expect(field.classList).toContain('mat-form-field-appearance-outline');
      }
    });

    it('sends what was typed and picked into the pickers', async () => {
      const { access, harness, page, element } = await open();
      const [startDate, startTime, endTime] = pickerInputs(element);
      typeInto(harness, startDate, '10/31/2026');
      typeInto(harness, startTime, '10:00 PM');
      typeInto(harness, endTime, '3:00 AM');
      // A pick from the calendar sets the picker's own control.
      const endDatePicker = harness.routeDebugElement!.queryAll(
        By.directive(SimpleDateComponent),
      )[3].componentInstance as SimpleDateComponent;
      endDatePicker.dateControl.setValue(new Date(2026, 10, 1));
      expect(page.dateRows.at(0).getRawValue()).toEqual({
        date: '2026-10-31',
        startTime: '22:00',
        endDate: '2026-11-01',
        endTime: '03:00',
      });

      page.form.patchValue({ title: 'Leather Night', agreed: true });
      page.onVenueSelected({ option: { value: LANTERN_ROOM } } as never);
      page.send();
      expect(access.submit.mock.calls[0][0].dates).toEqual([
        {
          starts_at_us: Date.UTC(2026, 10, 1, 5) * 1000,
          ends_at_us: Date.UTC(2026, 10, 1, 11) * 1000,
        },
      ]);
    });

    it('says under a date when its end is the next day, and only then', async () => {
      const { harness, element } = await open();
      const note = () => element.querySelector('.submit-event__end-note');
      const [startDate, startTime, endTime] = pickerInputs(element);
      typeInto(harness, startDate, '10/30/2026');
      typeInto(harness, startTime, '7:00 PM');
      typeInto(harness, endTime, '9:00 PM');
      expect(note()).toBeNull();
      typeInto(harness, startTime, '10:00 PM');
      typeInto(harness, endTime, '2:00 AM');
      expect(text(note())).toBe('Ends the next day: Sat, Oct 31, 2:00 AM.');
    });

    it('clears a date or time that is erased or unreadable', async () => {
      const { harness, page, element } = await open();
      const [startDate, , endTime] = pickerInputs(element);
      typeInto(harness, endTime, '3:00 AM');
      expect(page.dateRows.at(0).controls.endTime.value).toBe('03:00');
      typeInto(harness, endTime, '');
      expect(page.dateRows.at(0).controls.endTime.value).toBe('');
      typeInto(harness, endTime, 'late');
      expect(page.dateRows.at(0).controls.endTime.value).toBe('');

      typeInto(harness, startDate, '10/31/2026');
      typeInto(harness, startDate, '');
      expect(page.dateRows.at(0).controls.date.value).toBe('');
    });

    it('checks the form before sending, beside the field', async () => {
      const { access, harness, page, element } = await open();
      page.send();
      harness.detectChanges();
      expect(access.submit).not.toHaveBeenCalled();
      expect(text(element.querySelector('[data-field="title"]'))).toBe('Give the event a title.');

      fillIn(page);
      page.addDate();
      page.dateRows.at(1).patchValue({ date: '2026-11-07' });
      page.send();
      harness.detectChanges();
      expect(element.querySelector('[data-field="title"]')).toBeNull();
      expect(text(element.querySelector('[data-date-index="1"]'))).toBe(
        'Enter a start date and time.',
      );
    });
  });

  describe('a refusal from the server', () => {
    const cases: [string, SubmissionProblem, string][] = [
      [
        'the title',
        refusal({ error_code: 'TITLE_LENGTH', field: 'title' }),
        '[data-field="title"]',
      ],
      [
        'one date',
        refusal({ error_code: 'START_TOO_SOON', field: 'dates', date_index: 0 }),
        '[data-date-index="0"]',
      ],
      [
        'the dates',
        refusal({ error_code: 'TOO_MANY_DATES', field: 'dates' }),
        '[data-field="dates"]',
      ],
      [
        'the venue',
        refusal({ error_code: 'VENUE_NOT_FOUND', field: 'venue_id' }),
        '[data-field="venue_id"]',
      ],
      ['the link', refusal({ error_code: 'BAD_URL', field: 'url' }), '[data-field="url"]'],
      [
        'the cost',
        refusal({ error_code: 'COST_LENGTH', field: 'cost_text' }),
        '[data-field="cost_text"]',
      ],
      [
        'the categories',
        refusal({ error_code: 'TOO_MANY_TAGS', field: 'category_slugs' }),
        '[data-field="category_slugs"]',
      ],
      [
        'the scenes',
        refusal({ error_code: 'TOO_MANY_TAGS', field: 'scene_tag_slugs' }),
        '[data-field="scene_tag_slugs"]',
      ],
      [
        'the agreement',
        refusal({ error_code: 'NOT_AGREED', field: 'agreed' }),
        '[data-field="agreed"]',
      ],
      [
        'a rate limit',
        refusal({ status: 429, error_code: 'RATE_LIMITED', field: 'max_pending_dates' }),
        '.submit-event__form-error',
      ],
    ];

    for (const [name, problem, selector] of cases) {
      it(`shows ${name}'s problem in its place`, async () => {
        const { harness, page, element } = await open({
          submit: () => throwError(() => ({ ...problem, detail: `About ${name}` })),
        });
        fillIn(page);
        page.send();
        harness.detectChanges();
        expect(text(element.querySelector(selector))).toBe(`About ${name}`);
        expect(element.querySelectorAll('.submit-event__error').length).toBe(1);
        // What was typed stays.
        expect(page.form.controls.title.value).toBe('Leather Night');
      });
    }

    it('shows the unverified-email notice', async () => {
      const { harness, page, element } = await open({
        submit: () => throwError(() => refusal({ status: 403, error_code: 'EMAIL_NOT_VERIFIED' })),
      });
      fillIn(page);
      page.send();
      harness.detectChanges();
      expect(text(element.querySelector('.submit-event__unverified'))).toContain(
        'Confirm your email address first.',
      );
      expect(element.querySelector('form')).toBeNull();
    });

    it('shows the paused notice on a 503, and keeps what was typed', async () => {
      const { harness, page, element } = await open({
        accepting: [true, true],
        submit: () =>
          throwError(() =>
            refusal({
              status: 503,
              error_code: 'SUBMISSIONS_PAUSED',
              detail: "We're not taking event submissions right now. Please check back soon.",
            }),
          ),
      });
      fillIn(page);
      page.form.controls.description.setValue('Coat check at the door.');
      page.send();
      harness.detectChanges();
      expect(element.querySelector('.submit-event__paused')).not.toBeNull();
      expect(element.querySelector('form')).toBeNull();

      page.checkStatus();
      harness.detectChanges();
      const title = element.querySelector<HTMLInputElement>('input[formcontrolname="title"]')!;
      expect(title.value).toBe('Leather Night');
      expect(page.form.controls.description.value).toBe('Coat check at the door.');
      expect(page.venueId()).toBe(1);
      // The date pickers come back showing the row too.
      expect(pickerInputs(element).map(shown)).toEqual(['10/31/2026', '10:00 PM', '3:00 AM', '']);
      expect(page.dateRows.at(0).getRawValue()).toEqual({
        date: '2026-10-31',
        startTime: '22:00',
        endDate: '',
        endTime: '03:00',
      });
    });
  });

  describe('after sending', () => {
    it('thanks them and shows the listed events that look the same', async () => {
      const { harness, page, element } = await open();
      fillIn(page);
      page.send();
      harness.detectChanges();
      const sent = element.querySelector('.submit-event__sent')!;
      expect(text(sent)).toContain("Thanks, it's waiting for review.");
      const duplicate = sent.querySelector('.submit-event__duplicates li')!;
      expect(text(duplicate)).toBe('Leather Night · Fri, Aug 14, 9:00 PM (your date 2)');
      expect(duplicate.querySelector('a')?.getAttribute('href')).toBe('/events/103');
      expect(
        Array.from(sent.querySelectorAll('a'))
          .find((link) => text(link) === 'My submissions')
          ?.getAttribute('href'),
      ).toBe('/account/submissions');
    });

    it('starts a fresh form for another', async () => {
      const { harness, page, element } = await open();
      fillIn(page);
      page.addDate();
      page.send();
      page.startOver();
      harness.detectChanges();
      expect(element.querySelector('.submit-event__sent')).toBeNull();
      expect(page.form.controls.title.value).toBe('');
      expect(page.dateRows.length).toBe(1);
      expect(page.venueId()).toBeNull();
    });
  });
});
