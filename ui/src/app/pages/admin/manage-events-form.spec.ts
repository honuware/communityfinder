import { pacificWallClockToUs } from '../../core/utils/events-date.utils';
import {
  assignmentChanges,
  composeEventTimes,
  EMPTY_EVENT_FORM,
  eventFormFromRow,
  EventFormValue,
  eventRowValue,
  rowChanges,
  rowForCreate,
} from './manage-events-form';

function utc(year: number, month: number, day: number, hour = 0, minute = 0): number {
  return Date.UTC(year, month - 1, day, hour, minute) * 1000;
}

function form(fields: Partial<EventFormValue>): EventFormValue {
  return { ...EMPTY_EVENT_FORM, title: 'Leather Night', ...fields };
}

describe('manage-events-form', () => {
  describe('composeEventTimes', () => {
    it('turns Seattle wall-clock inputs into instants, summer and winter', () => {
      expect(composeEventTimes(form({ startDate: '2026-08-14', startTime: '21:00' }))).toEqual({
        ok: true,
        startsAtUs: utc(2026, 8, 15, 4),
        endsAtUs: null,
      });
      expect(composeEventTimes(form({ startDate: '2026-12-24', startTime: '19:30' }))).toEqual({
        ok: true,
        startsAtUs: utc(2026, 12, 25, 3, 30),
        endsAtUs: null,
      });
    });

    it('gets both 2026 DST weekends right', () => {
      // Spring forward: Sat Mar 7 is PST, Sun Mar 8 PDT.
      const spring = composeEventTimes(
        form({ startDate: '2026-03-07', startTime: '22:00', endTime: '03:00' }),
      );
      expect(spring).toEqual({
        ok: true,
        startsAtUs: utc(2026, 3, 8, 6),
        endsAtUs: utc(2026, 3, 8, 10),
      });
      // Fall back: Sat Oct 31 is PDT, Sun Nov 1 PST.
      const fall = composeEventTimes(
        form({ startDate: '2026-10-31', startTime: '22:00', endTime: '03:00' }),
      );
      expect(fall).toEqual({
        ok: true,
        startsAtUs: utc(2026, 11, 1, 5),
        endsAtUs: utc(2026, 11, 1, 11),
      });
    });

    it('reads an end time alone as the same day, or the next morning when earlier', () => {
      const evening = composeEventTimes(
        form({ startDate: '2026-08-14', startTime: '18:00', endTime: '21:00' }),
      );
      expect(evening).toMatchObject({ ok: true, endsAtUs: utc(2026, 8, 15, 4) });
      const lateNight = composeEventTimes(
        form({ startDate: '2026-08-14', startTime: '22:00', endTime: '02:00' }),
      );
      expect(lateNight).toMatchObject({ ok: true, endsAtUs: utc(2026, 8, 15, 9) });
      // Across a month end.
      const monthEnd = composeEventTimes(
        form({ startDate: '2026-08-31', startTime: '22:00', endTime: '01:00' }),
      );
      expect(monthEnd).toMatchObject({ ok: true, endsAtUs: utc(2026, 9, 1, 8) });
    });

    it('takes an end date for a multi-day event', () => {
      const festival = composeEventTimes(
        form({
          startDate: '2026-08-14',
          startTime: '10:00',
          endDate: '2026-08-16',
          endTime: '18:00',
        }),
      );
      expect(festival).toMatchObject({ ok: true, endsAtUs: utc(2026, 8, 17, 1) });
    });

    it('explains what is wrong', () => {
      expect(composeEventTimes(form({ startDate: '2026-08-14' }))).toEqual({
        ok: false,
        error: 'Enter a start date and time.',
      });
      expect(
        composeEventTimes(
          form({ startDate: '2026-08-14', startTime: '20:00', endDate: '2026-08-14' }),
        ),
      ).toMatchObject({ ok: false });
      expect(
        composeEventTimes(
          form({
            startDate: '2026-08-14',
            startTime: '20:00',
            endDate: '2026-08-14',
            endTime: '19:00',
          }),
        ),
      ).toEqual({ ok: false, error: 'The end must be after the start.' });
    });
  });

  it('loads an events row into the form in Seattle time', () => {
    const row = {
      id: '7',
      title: 'Leather Night',
      description: 'Coat check at the door.',
      starts_at_us: String(pacificWallClockToUs(2026, 8, 14, 21)),
      ends_at_us: String(pacificWallClockToUs(2026, 8, 15, 2)),
      venue_id: '1',
      series_id: '',
      location_text: '',
      url: '',
      cost_text: '$10 cover',
    };
    expect(eventFormFromRow(row)).toEqual({
      title: 'Leather Night',
      description: 'Coat check at the door.',
      startDate: '2026-08-14',
      startTime: '21:00',
      endDate: '2026-08-15',
      endTime: '02:00',
      venueId: '1',
      seriesId: '',
      locationText: '',
      url: '',
      costText: '$10 cover',
    });
    expect(eventFormFromRow({ ...row, ends_at_us: '' })).toMatchObject({
      endDate: '',
      endTime: '',
    });
  });

  describe('the row it writes', () => {
    const times = { startsAtUs: 1_000, endsAtUs: null };

    it('leaves blank number columns out of a new pending row, and records who entered it', () => {
      const row = eventRowValue(form({ title: '  Leather Night ', costText: 'Free' }), times);
      expect(rowForCreate(row, 3)).toEqual({
        title: 'Leather Night',
        description: '',
        starts_at_us: '1000',
        location_text: '',
        url: '',
        cost_text: 'Free',
        status: 'pending',
        created_by_person_id: '3',
      });
      expect(rowForCreate(row, null)['created_by_person_id']).toBeUndefined();
    });

    it('sends only what changed on an edit, a cleared number column as blank', () => {
      const original = {
        id: '7',
        title: 'Leather Night',
        description: '',
        starts_at_us: '1000',
        ends_at_us: '5000',
        venue_id: '1',
        series_id: '',
        location_text: '',
        url: '',
        cost_text: '',
        status: 'approved',
      };
      const edited = eventRowValue(form({ venueId: '', costText: '$10' }), times);
      expect(rowChanges(edited, original)).toEqual({
        ends_at_us: '',
        venue_id: '',
        cost_text: '$10',
      });
    });

    it('sends an emptied text column as blank', () => {
      const original = { title: 'Leather Night', description: 'Dress code encouraged.' };
      expect(rowChanges({ title: 'Leather Night', description: '' }, original)).toEqual({
        description: '',
      });
    });
  });

  describe('assignmentChanges', () => {
    const existing = [
      { id: 'a', tagId: 1 },
      { id: 'b', tagId: 2 },
      // A turned-off category: not on the form.
      { id: 'c', tagId: 99 },
    ];

    it('adds the newly checked and deletes the unchecked', () => {
      expect(assignmentChanges(existing, [1, 2, 3], [1, 3])).toEqual({ add: [3], remove: ['b'] });
    });

    it('leaves an assignment the form does not show alone', () => {
      expect(assignmentChanges(existing, [1, 2, 3], []).remove).toEqual(['a', 'b']);
    });

    it('writes nothing for an event left with no tags of its own', () => {
      expect(assignmentChanges([], [1, 2, 3], [])).toEqual({ add: [], remove: [] });
    });
  });
});
