import { ComponentFixture, TestBed } from '@angular/core/testing';
import { provideRouter } from '@angular/router';
import { By } from '@angular/platform-browser';
import { provideNoopAnimations } from '@angular/platform-browser/animations';
import { of, throwError } from 'rxjs';
import { vi } from 'vitest';
import { ToastService } from '@honuware/ui/foundation';
import { ImageTooLargeError, PhotoUploadComponent } from '@honuware/ui/photos';
import { provideHonuwareAccessMock } from '@honuware/ui/testing';
import { FLYER_UNREADABLE_MESSAGE } from '../events/submit-event-form';
import {
  EVENT_SUBMISSIONS_ACCESS,
  EventSubmissionsAccess,
  MySubmission,
  MySubmissionDate,
  SubmissionDateState,
} from '../../core/event-submissions-access/event-submissions-access';
import { pacificWallClockToUs } from '../../core/utils/events-date.utils';
import { MySubmissionsPage } from './my-submissions';

const FRIDAY_9PM = pacificWallClockToUs(2026, 8, 14, 21);
const HOUR_US = 3_600 * 1_000_000;
const DAY_US = 24 * HOUR_US;

function date(
  eventId: number,
  state: SubmissionDateState,
  dayOffset = 0,
  fields: Partial<MySubmissionDate> = {},
): MySubmissionDate {
  return {
    event_id: eventId,
    title: 'Queer Craft Night',
    starts_at_us: FRIDAY_9PM + dayOffset * DAY_US,
    ends_at_us: FRIDAY_9PM + dayOffset * DAY_US + 2 * HOUR_US,
    state,
    has_photo: false,
    video: null,
    ...fields,
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
  submission(
    4,
    [date(41, 'listed', 0, { has_photo: true, video: { provider: 'vimeo', id: '76979871' } })],
    { relationship: 'attendee' },
  ),
  submission(3, [date(31, 'not_listed')], {
    reject_reason: 'missing-details',
    message_to_submitter: 'Which room is it in?',
  }),
  submission(2, [date(21, 'withdrawn')], { reject_reason: 'withdrawn' }),
  submission(1, [date(11, 'past')], { reject_reason: 'expired' }),
];

function render(
  mine: MySubmission[] | Error = MINE,
  overrides: Partial<Record<keyof EventSubmissionsAccess, ReturnType<typeof vi.fn>>> = {},
) {
  const access = {
    getStatus: vi.fn(() => of(true)),
    submit: vi.fn(),
    getMine: vi.fn(() => (mine instanceof Error ? throwError(() => mine) : of(mine))),
    withdraw: vi.fn(),
    attachFlyer: vi.fn(),
    removeFlyer: vi.fn(),
    ...overrides,
  };
  TestBed.configureTestingModule({
    imports: [MySubmissionsPage],
    providers: [
      provideRouter([]),
      provideNoopAnimations(),
      // The flyer picker's own photo access, which a held-back pick never calls.
      provideHonuwareAccessMock({}),
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

  // CM3 §5.7.
  it('says when a date came with a photo or a video, and nothing when not', () => {
    const { element } = render();
    expect(text(card(element, 4).querySelector('.my-submissions__extras'))).toBe(
      'With a photo · With a Vimeo video',
    );
    expect(card(element, 5).querySelector('.my-submissions__extras')).toBeNull();
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
    const { fixture, element } = render(MINE, { withdraw });
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
    const { fixture, page, element } = render(MINE, { withdraw });
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
    const { fixture, page, element } = render(MINE, { withdraw });
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

  // CM3 Phase 2 §11 (OQ14): the flyer, added, replaced or removed while a
  // date waits.
  describe('the flyer', () => {
    // Submission 6: one waiting date with the flyer on it, and one listed.
    const WITH_FLYER = submission(6, [
      date(61, 'waiting', 0, { has_photo: true }),
      date(62, 'listed', 7, { has_photo: true }),
    ]);

    function picker(fixture: ComponentFixture<MySubmissionsPage>): PhotoUploadComponent {
      return fixture.debugElement.query(By.directive(PhotoUploadComponent))
        .componentInstance as PhotoUploadComponent;
    }

    // jsdom has no canvas to shrink a flyer on, so the picker's prepare step
    // (CM3 Phase 2 §13.3; @honuware/ui's own specs cover it) is stood in for:
    // it hands back these bytes as the JPEG, or fails with `failure`.
    const PREPARED_FLYER = [9, 8, 7];

    function choose(
      fixture: ComponentFixture<MySubmissionsPage>,
      name: string,
      type: string,
      failure?: unknown,
    ) {
      const flyerPicker = picker(fixture);
      flyerPicker.pendingFile = new File([new Uint8Array([1, 2, 3, 4])], name, { type });
      vi.spyOn(flyerPicker, 'preparePendingPhoto').mockReturnValue(
        failure
          ? throwError(() => failure)
          : of({ buffer: new Uint8Array(PREPARED_FLYER).buffer, imageType: 'jpeg' as const }),
      );
    }

    function buttonIn(within: Element, label: string): HTMLButtonElement {
      return Array.from(within.querySelectorAll('button')).find((entry) => text(entry) === label)!;
    }

    it('offers to add one only while a date waits, and to replace or remove one once it’s on', () => {
      const { element } = render([...MINE, WITH_FLYER]);
      expect(text(card(element, 5).querySelector('.my-submissions__flyer-add'))).toBe(
        'Add a flyer',
      );
      expect(card(element, 5).querySelector('.my-submissions__flyer-remove')).toBeNull();
      expect(text(card(element, 6).querySelector('.my-submissions__flyer-add'))).toBe(
        'Replace the flyer',
      );
      expect(text(card(element, 6).querySelector('.my-submissions__flyer-remove'))).toBe(
        'Remove the flyer',
      );
      for (const id of [4, 3, 2, 1]) {
        expect(card(element, id).querySelector('.my-submissions__flyer-add')).toBeNull();
        expect(card(element, id).querySelector('.my-submissions__flyer-remove')).toBeNull();
      }
    });

    it('adds one with the picker, and says each waiting date has a photo', async () => {
      const attachFlyer = vi.fn(() => of({ submission_id: 5, event_ids: [51, 52] }));
      const { fixture, element } = render(MINE, { attachFlyer });
      const success = vi.spyOn(TestBed.inject(ToastService), 'success');
      card(element, 5).querySelector<HTMLButtonElement>('.my-submissions__flyer-add')!.click();
      fixture.detectChanges();
      const panel = card(element, 5).querySelector('.my-submissions__flyer')!;
      expect(panel.querySelector('hw-photo-upload')).not.toBeNull();
      choose(fixture, 'flyer.png', 'image/png');
      buttonIn(panel, 'Add it').click();
      await vi.waitFor(() => expect(attachFlyer).toHaveBeenCalledTimes(1));
      // Shrunk and re-encoded as a JPEG first, as the submit form does.
      expect(attachFlyer).toHaveBeenCalledWith(5, expect.any(ArrayBuffer), 'jpeg');
      const sent = (attachFlyer.mock.calls[0] as unknown[])[1] as ArrayBuffer;
      expect(Array.from(new Uint8Array(sent))).toEqual(PREPARED_FLYER);
      fixture.detectChanges();
      expect(card(element, 5).querySelector('.my-submissions__flyer')).toBeNull();
      const extras = Array.from(card(element, 5).querySelectorAll('.my-submissions__extras'));
      expect(extras.map((entry) => text(entry))).toEqual(['With a photo', 'With a photo']);
      expect(text(card(element, 5).querySelector('.my-submissions__flyer-add'))).toBe(
        'Replace the flyer',
      );
      expect(success).toHaveBeenCalledWith('Your flyer is attached.');
    });

    it('replaces one the same way', async () => {
      const attachFlyer = vi.fn(() => of({ submission_id: 6, event_ids: [61] }));
      const { fixture, element } = render([WITH_FLYER], { attachFlyer });
      card(element, 6).querySelector<HTMLButtonElement>('.my-submissions__flyer-add')!.click();
      fixture.detectChanges();
      choose(fixture, 'new-flyer.jpg', 'image/jpeg');
      buttonIn(card(element, 6).querySelector('.my-submissions__flyer')!, 'Replace it').click();
      await vi.waitFor(() =>
        expect(attachFlyer).toHaveBeenCalledWith(6, expect.any(ArrayBuffer), 'jpeg'),
      );
    });

    it('refuses anything but a JPEG or PNG, and asks for a file, before sending', () => {
      const attachFlyer = vi.fn();
      const { fixture, element } = render(MINE, { attachFlyer });
      card(element, 5).querySelector<HTMLButtonElement>('.my-submissions__flyer-add')!.click();
      fixture.detectChanges();
      const panel = () => card(element, 5).querySelector('.my-submissions__flyer')!;
      buttonIn(panel(), 'Add it').click();
      fixture.detectChanges();
      expect(text(panel().querySelector('.my-submissions__error'))).toBe(
        'Choose the flyer to add.',
      );
      choose(fixture, 'flyer.svg', 'image/svg+xml');
      buttonIn(panel(), 'Add it').click();
      fixture.detectChanges();
      expect(text(panel().querySelector('.my-submissions__error'))).toBe(
        'The flyer must be a JPEG or PNG image.',
      );
      expect(attachFlyer).not.toHaveBeenCalled();
    });

    it('says a flyer is still too big after shrinking, or can’t be read, and sends nothing', () => {
      const attachFlyer = vi.fn();
      const { fixture, element } = render(MINE, { attachFlyer });
      card(element, 5).querySelector<HTMLButtonElement>('.my-submissions__flyer-add')!.click();
      fixture.detectChanges();
      const panel = () => card(element, 5).querySelector('.my-submissions__flyer')!;

      choose(
        fixture,
        'flyer.png',
        'image/png',
        new ImageTooLargeError(12 * 1024 * 1024, 10 * 1024 * 1024),
      );
      buttonIn(panel(), 'Add it').click();
      fixture.detectChanges();
      expect(text(panel().querySelector('.my-submissions__error'))).toBe(
        'This image is 12 MB even after shrinking, and the limit is 10 MB. Try a smaller image.',
      );

      choose(fixture, 'flyer.jpg', 'image/jpeg', new Error('Failed to load image'));
      buttonIn(panel(), 'Add it').click();
      fixture.detectChanges();
      expect(text(panel().querySelector('.my-submissions__error'))).toBe(FLYER_UNREADABLE_MESSAGE);
      expect(attachFlyer).not.toHaveBeenCalled();
      expect(card(element, 5).querySelector('.my-submissions__flyer')).not.toBeNull();
    });

    it('keeps the picker open with the server’s word when the flyer is refused', async () => {
      const attachFlyer = vi.fn(() =>
        throwError(() => ({
          status: 400,
          error_code: 'IMAGE_TOO_LARGE',
          field: '',
          date_index: null,
          detail: 'The flyer is too large.',
        })),
      );
      const { fixture, element } = render(MINE, { attachFlyer });
      card(element, 5).querySelector<HTMLButtonElement>('.my-submissions__flyer-add')!.click();
      fixture.detectChanges();
      choose(fixture, 'flyer.png', 'image/png');
      buttonIn(card(element, 5).querySelector('.my-submissions__flyer')!, 'Add it').click();
      await vi.waitFor(() => expect(attachFlyer).toHaveBeenCalled());
      fixture.detectChanges();
      const panel = card(element, 5).querySelector('.my-submissions__flyer')!;
      expect(text(panel.querySelector('.my-submissions__error'))).toBe('The flyer is too large.');
      expect(card(element, 5).querySelector('.my-submissions__extras')).toBeNull();
    });

    it('Cancel closes the picker and sends nothing', () => {
      const attachFlyer = vi.fn();
      const { fixture, element } = render(MINE, { attachFlyer });
      card(element, 5).querySelector<HTMLButtonElement>('.my-submissions__flyer-add')!.click();
      fixture.detectChanges();
      buttonIn(card(element, 5).querySelector('.my-submissions__flyer')!, 'Cancel').click();
      fixture.detectChanges();
      expect(card(element, 5).querySelector('.my-submissions__flyer')).toBeNull();
      expect(attachFlyer).not.toHaveBeenCalled();
    });

    it('asks in the page before removing one, then takes it off the waiting date only', () => {
      const removeFlyer = vi.fn(() => of({ submission_id: 6, event_ids: [61] }));
      const { fixture, element } = render([WITH_FLYER], { removeFlyer });
      const success = vi.spyOn(TestBed.inject(ToastService), 'success');
      card(element, 6).querySelector<HTMLButtonElement>('.my-submissions__flyer-remove')!.click();
      fixture.detectChanges();
      const confirm = card(element, 6).querySelector('[role="alertdialog"]')!;
      expect(text(confirm.querySelector('p'))).toBe(
        'Remove the flyer? It comes off your 1 waiting date. Anything already listed keeps its photo.',
      );
      expect(removeFlyer).not.toHaveBeenCalled();
      buttonIn(confirm, 'Remove it').click();
      fixture.detectChanges();
      expect(removeFlyer).toHaveBeenCalledWith(6);
      const extras = Array.from(card(element, 6).querySelectorAll('.my-submissions__date')).map(
        (row) => text(row.querySelector('.my-submissions__extras')),
      );
      expect(extras).toEqual(['', 'With a photo']);
      expect(text(card(element, 6).querySelector('.my-submissions__flyer-add'))).toBe(
        'Add a flyer',
      );
      expect(card(element, 6).querySelector('.my-submissions__flyer-remove')).toBeNull();
      expect(success).toHaveBeenCalledWith('Flyer removed.');
    });

    it('keeps the flyer when the confirm is turned down, or the removal is refused', () => {
      const removeFlyer = vi.fn(() =>
        throwError(() => ({
          status: 400,
          error_code: 'NOTHING_PENDING',
          field: '',
          date_index: null,
          detail: 'Nothing in this submission is waiting for review.',
        })),
      );
      const { fixture, page, element } = render([WITH_FLYER], { removeFlyer });
      const error = vi.spyOn(TestBed.inject(ToastService), 'error');
      card(element, 6).querySelector<HTMLButtonElement>('.my-submissions__flyer-remove')!.click();
      fixture.detectChanges();
      buttonIn(card(element, 6).querySelector('[role="alertdialog"]')!, 'Keep it').click();
      fixture.detectChanges();
      expect(removeFlyer).not.toHaveBeenCalled();
      expect(card(element, 6).querySelector('[role="alertdialog"]')).toBeNull();

      page.removeFlyer(WITH_FLYER);
      fixture.detectChanges();
      expect(error).toHaveBeenCalledWith('Nothing in this submission is waiting for review.');
      expect(card(element, 6).querySelector('.my-submissions__flyer-remove')).not.toBeNull();
    });

    it('opens one panel at a time', () => {
      const { fixture, element } = render([...MINE, WITH_FLYER]);
      card(element, 5).querySelector<HTMLButtonElement>('.my-submissions__flyer-add')!.click();
      fixture.detectChanges();
      card(element, 6).querySelector<HTMLButtonElement>('.my-submissions__withdraw')!.click();
      fixture.detectChanges();
      expect(element.querySelectorAll('.my-submissions__flyer').length).toBe(0);
      expect(element.querySelectorAll('[role="alertdialog"]').length).toBe(1);
    });
  });
});
