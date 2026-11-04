import { TestBed } from '@angular/core/testing';
import { provideRouter } from '@angular/router';
import { provideNoopAnimations } from '@angular/platform-browser/animations';
import { of, throwError } from 'rxjs';
import { vi } from 'vitest';
import { ToastService } from '@honuware/ui/foundation';
import {
  EVENT_SUBMISSIONS_ACCESS,
  MySubmission,
  MySubmissionDate,
  SubmissionDateState,
} from '../../core/event-submissions-access/event-submissions-access';
import { pacificWallClockToUs } from '../../core/utils/events-date.utils';
import { MySubmissionsPage } from './my-submissions';

const FRIDAY_9PM = pacificWallClockToUs(2026, 8, 14, 21);
const HOUR_US = 3_600 * 1_000_000;
const DAY_US = 24 * HOUR_US;

function date(eventId: number, state: SubmissionDateState, dayOffset = 0): MySubmissionDate {
  return {
    event_id: eventId,
    title: 'Queer Craft Night',
    starts_at_us: FRIDAY_9PM + dayOffset * DAY_US,
    ends_at_us: FRIDAY_9PM + dayOffset * DAY_US + 2 * HOUR_US,
    state,
  };
}

function submission(id: number, dates: MySubmissionDate[], fields: Partial<MySubmission> = {}) {
  return {
    id,
    created_us: pacificWallClockToUs(2026, 8, 10, 12),
    relationship: 'organizer',
    regular_thing: false,
    submitter_note: '',
    reject_reason: '',
    message_to_submitter: '',
    dates,
    ...fields,
  } as MySubmission;
}

// One submission per state, plus a mixed one that can be withdrawn.
const MINE: MySubmission[] = [
  submission(5, [date(51, 'waiting'), date(52, 'waiting', 7), date(53, 'listed', 14)], {
    regular_thing: true,
  }),
  submission(4, [date(41, 'listed')], { relationship: 'attendee' }),
  submission(3, [date(31, 'not_listed')], {
    reject_reason: 'missing-details',
    message_to_submitter: 'Which room is it in?',
  }),
  submission(2, [date(21, 'withdrawn')], { reject_reason: 'withdrawn' }),
  submission(1, [date(11, 'past')], { reject_reason: 'expired' }),
];

function render(mine: MySubmission[] | Error = MINE, withdraw = vi.fn()) {
  const access = {
    getStatus: vi.fn(() => of(true)),
    submit: vi.fn(),
    getMine: vi.fn(() => (mine instanceof Error ? throwError(() => mine) : of(mine))),
    withdraw,
  };
  TestBed.configureTestingModule({
    imports: [MySubmissionsPage],
    providers: [
      provideRouter([]),
      provideNoopAnimations(),
      { provide: EVENT_SUBMISSIONS_ACCESS, useValue: access },
    ],
  });
  const fixture = TestBed.createComponent(MySubmissionsPage);
  fixture.detectChanges();
  return {
    access,
    fixture,
    page: fixture.componentInstance,
    element: fixture.nativeElement as HTMLElement,
  };
}

function text(element: Element | null | undefined): string {
  return element?.textContent?.replace(/\s+/g, ' ').trim() ?? '';
}

function card(element: HTMLElement, id: number): Element {
  return element.querySelector(`[data-id="${id}"]`)!;
}

function states(element: HTMLElement, id: number): string[] {
  return Array.from(card(element, id).querySelectorAll('.my-submissions__state')).map((chip) =>
    text(chip),
  );
}

describe('MySubmissionsPage', () => {
  afterEach(() => vi.restoreAllMocks());

  it('shows each submission newest first, with its dates and when they are', () => {
    const { element } = render();
    const cards = Array.from(element.querySelectorAll('.my-submissions__card'));
    expect(cards.map((entry) => entry.getAttribute('data-id'))).toEqual(['5', '4', '3', '2', '1']);
    expect(text(card(element, 5).querySelector('h2'))).toBe('Queer Craft Night (3 dates)');
    expect(text(card(element, 4).querySelector('h2'))).toBe('Queer Craft Night');
    expect(text(card(element, 5))).toContain(
      "Sent Mon, Aug 10 · I'm the organizer · A regular thing",
    );
    expect(text(card(element, 4))).toContain("I'm going");
    expect(text(card(element, 4).querySelector('.my-submissions__when'))).toContain('Fri, Aug 14');
  });

  it('labels each state', () => {
    const { element } = render();
    expect(states(element, 5)).toEqual(['Waiting for review', 'Waiting for review', 'Listed']);
    expect(states(element, 4)).toEqual(['Listed']);
    expect(states(element, 3)).toEqual(['Not listed']);
    expect(states(element, 2)).toEqual(['Withdrawn']);
    expect(states(element, 1)).toEqual(['Past']);
    expect(card(element, 3).querySelector('.my-submissions__state--not_listed')).not.toBeNull();
  });

  it('links a listed date to its page', () => {
    const { element } = render();
    const links = Array.from(card(element, 5).querySelectorAll('.my-submissions__date a'));
    expect(links.map((link) => [text(link), link.getAttribute('href')])).toEqual([
      ['See it', '/events/53'],
    ]);
    expect(card(element, 3).querySelector('.my-submissions__date a')).toBeNull();
  });

  it('says why one was not listed, with the curator’s message', () => {
    const { element } = render();
    expect(text(card(element, 3).querySelector('.my-submissions__reason'))).toBe(
      'Why: We needed more details to list it.',
    );
    expect(text(card(element, 3).querySelector('.my-submissions__message'))).toBe(
      'Which room is it in?',
    );
    expect(text(card(element, 1).querySelector('.my-submissions__reason'))).toBe(
      "Why: We didn't get to it before the date passed. Sorry about that.",
    );
    // A withdrawal is the submitter's own doing: no "why".
    expect(card(element, 2).querySelector('.my-submissions__reason')).toBeNull();
    expect(card(element, 4).querySelector('.my-submissions__reason')).toBeNull();
  });

  it('offers Withdraw only while a date is waiting', () => {
    const { element } = render();
    expect(card(element, 5).querySelector('.my-submissions__withdraw')).not.toBeNull();
    for (const id of [4, 3, 2, 1]) {
      expect(card(element, id).querySelector('.my-submissions__withdraw')).toBeNull();
    }
  });

  it('confirms in the page, then withdraws and updates in place', () => {
    const withdraw = vi.fn(() => of({ submission_id: 5, withdrawn_event_ids: [51, 52] }));
    const { fixture, element } = render(MINE, withdraw);
    const success = vi.spyOn(TestBed.inject(ToastService), 'success');
    card(element, 5).querySelector<HTMLButtonElement>('.my-submissions__withdraw')!.click();
    fixture.detectChanges();
    const confirm = card(element, 5).querySelector('[role="alertdialog"]')!;
    expect(text(confirm)).toContain('Its 2 waiting dates will be taken out of the queue.');
    expect(withdraw).not.toHaveBeenCalled();

    Array.from(confirm.querySelectorAll('button'))
      .find((button) => text(button) === 'Withdraw it')!
      .click();
    fixture.detectChanges();
    expect(withdraw).toHaveBeenCalledWith(5);
    expect(states(element, 5)).toEqual(['Withdrawn', 'Withdrawn', 'Listed']);
    expect(card(element, 5).querySelector('.my-submissions__withdraw')).toBeNull();
    expect(card(element, 5).querySelector('[role="alertdialog"]')).toBeNull();
    expect(success).toHaveBeenCalledWith('Withdrawn. Those dates are out of the queue.');
  });

  it('keeps it when the confirm is turned down', () => {
    const withdraw = vi.fn();
    const { fixture, page, element } = render(MINE, withdraw);
    page.askToWithdraw(MINE[0]);
    fixture.detectChanges();
    Array.from(card(element, 5).querySelectorAll('button'))
      .find((button) => text(button) === 'Keep it')!
      .click();
    fixture.detectChanges();
    expect(withdraw).not.toHaveBeenCalled();
    expect(card(element, 5).querySelector('[role="alertdialog"]')).toBeNull();
    expect(states(element, 5)).toEqual(['Waiting for review', 'Waiting for review', 'Listed']);
  });

  it('says what went wrong when a withdraw is refused', () => {
    const withdraw = vi.fn(() =>
      throwError(() => ({
        status: 400,
        error_code: 'NOTHING_PENDING',
        field: '',
        date_index: null,
        detail: 'Nothing in this submission is waiting for review',
      })),
    );
    const { fixture, page, element } = render(MINE, withdraw);
    const error = vi.spyOn(TestBed.inject(ToastService), 'error');
    page.withdraw(MINE[0]);
    fixture.detectChanges();
    expect(error).toHaveBeenCalledWith('Nothing in this submission is waiting for review');
    expect(states(element, 5)).toEqual(['Waiting for review', 'Waiting for review', 'Listed']);
  });

  it('points to the form when there is nothing yet', () => {
    const { element } = render([]);
    const empty = element.querySelector('.my-submissions__empty')!;
    expect(text(empty)).toContain("You haven't submitted any events yet.");
    expect(empty.querySelector('a')?.getAttribute('href')).toBe('/events/submit');
  });

  it('says so when the list can’t be read', () => {
    const { element } = render(new Error('offline'));
    expect(text(element.querySelector('.my-submissions__empty'))).toBe(
      "Your submissions couldn't be loaded. Please try again.",
    );
  });
});
