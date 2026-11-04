import {
  composeSubmission,
  EMPTY_SUBMIT_DATE,
  EMPTY_SUBMIT_FORM,
  MAX_SUBMISSION_DATES,
  SubmitDateValue,
  SubmitFormValue,
} from './submit-event-form';

function utc(year: number, month: number, day: number, hour = 0, minute = 0): number {
  return Date.UTC(year, month - 1, day, hour, minute) * 1000;
}

function date(fields: Partial<SubmitDateValue>): SubmitDateValue {
  return { ...EMPTY_SUBMIT_DATE, ...fields };
}

const AUG_14 = date({ date: '2026-08-14', startTime: '21:00' });

// A form that sends as it is: a title, one date, a venue, and the agreement.
function form(fields: Partial<SubmitFormValue>): SubmitFormValue {
  return {
    ...EMPTY_SUBMIT_FORM,
    title: 'Leather Night',
    dates: [AUG_14],
    venueId: 1,
    agreed: true,
    ...fields,
  };
}

describe('submit-event-form', () => {
  describe('the dates', () => {
    it('turns several Seattle dates into instants, across both 2026 DST weekends', () => {
      const composed = composeSubmission(
        form({
          dates: [
            // Spring forward: Sat Mar 7 is PST, Sun Mar 8 PDT.
            date({ date: '2026-03-07', startTime: '22:00', endTime: '03:00' }),
            // Fall back: Sat Oct 31 is PDT, Sun Nov 1 PST.
            date({ date: '2026-10-31', startTime: '22:00', endTime: '03:00' }),
            // Winter, no end.
            date({ date: '2026-12-24', startTime: '19:30' }),
            // A festival ending on a later day.
            date({
              date: '2026-08-14',
              startTime: '10:00',
              endDate: '2026-08-16',
              endTime: '18:00',
            }),
          ],
        }),
      );
      expect(composed.ok).toBe(true);
      expect(composed.ok && composed.request.dates).toEqual([
        { starts_at_us: utc(2026, 3, 8, 6), ends_at_us: utc(2026, 3, 8, 10) },
        { starts_at_us: utc(2026, 11, 1, 5), ends_at_us: utc(2026, 11, 1, 11) },
        { starts_at_us: utc(2026, 12, 25, 3, 30) },
        { starts_at_us: utc(2026, 8, 14, 17), ends_at_us: utc(2026, 8, 17, 1) },
      ]);
    });

    it(`takes up to ${MAX_SUBMISSION_DATES} dates`, () => {
      const dates = Array.from({ length: MAX_SUBMISSION_DATES + 1 }, (_, index) =>
        date({ date: `2026-09-${String(index + 1).padStart(2, '0')}`, startTime: '19:00' }),
      );
      expect(composeSubmission(form({ dates: dates.slice(0, MAX_SUBMISSION_DATES) })).ok).toBe(
        true,
      );
      expect(composeSubmission(form({ dates }))).toEqual({
        ok: false,
        field: 'dates',
        dateIndex: null,
        message: 'A submission can have up to 12 dates.',
      });
      expect(composeSubmission(form({ dates: [] }))).toMatchObject({
        ok: false,
        field: 'dates',
        message: 'Add at least one date.',
      });
    });

    it('names the date that has a problem', () => {
      const composed = composeSubmission(
        form({
          dates: [
            AUG_14,
            date({ date: '2026-08-21' }),
            date({
              date: '2026-08-28',
              startTime: '20:00',
              endDate: '2026-08-28',
              endTime: '19:00',
            }),
          ],
        }),
      );
      expect(composed).toEqual({
        ok: false,
        field: 'dates',
        dateIndex: 1,
        message: 'Enter a start date and time.',
      });
      const late = composeSubmission(
        form({
          dates: [
            AUG_14,
            date({
              date: '2026-08-28',
              startTime: '20:00',
              endDate: '2026-08-28',
              endTime: '19:00',
            }),
          ],
        }),
      );
      expect(late).toEqual({
        ok: false,
        field: 'dates',
        dateIndex: 1,
        message: 'The end must be after the start.',
      });
    });
  });

  describe('the other fields', () => {
    it('asks for a title, a place and the agreement, in the form’s order', () => {
      expect(composeSubmission(form({ title: '   ' }))).toEqual({
        ok: false,
        field: 'title',
        dateIndex: null,
        message: 'Give the event a title.',
      });
      expect(composeSubmission(form({ venueId: null }))).toMatchObject({
        ok: false,
        field: 'venue_id',
        message: 'Pick a venue from the list, or choose "Somewhere else".',
      });
      expect(
        composeSubmission(form({ placeMode: 'elsewhere', venueId: null, locationText: ' ' })),
      ).toMatchObject({ ok: false, field: 'location_text' });
      expect(composeSubmission(form({ agreed: false }))).toMatchObject({
        ok: false,
        field: 'agreed',
        message: 'Please confirm the details are accurate and can be listed publicly.',
      });
      // The first problem wins.
      expect(composeSubmission(form({ title: '', agreed: false }))).toMatchObject({
        field: 'title',
      });
    });

    it('sends what was filled in, trimmed, and leaves the blanks out', () => {
      const composed = composeSubmission(
        form({
          title: '  Leather Night ',
          description: ' Coat check at the door. ',
          url: 'https://example.org/leather',
          costText: '',
          categorySlugs: ['bar'],
          sceneTagSlugs: [],
          regularThing: true,
          relationship: 'venue',
          submitterNote: '  ',
        }),
      );
      expect(composed).toEqual({
        ok: true,
        request: {
          title: 'Leather Night',
          dates: [{ starts_at_us: utc(2026, 8, 15, 4) }],
          venue_id: 1,
          description: 'Coat check at the door.',
          url: 'https://example.org/leather',
          category_slugs: ['bar'],
          regular_thing: true,
          relationship: 'venue',
          agreed: true,
        },
      });
    });

    it('sends only the chosen kind of place', () => {
      const elsewhere = composeSubmission(
        form({
          placeMode: 'elsewhere',
          venueId: 1,
          locationText: ' Volunteer Park, by the tower ',
        }),
      );
      expect(elsewhere.ok && elsewhere.request.venue_id).toBeUndefined();
      expect(elsewhere.ok && elsewhere.request.location_text).toBe('Volunteer Park, by the tower');

      const venue = composeSubmission(form({ locationText: 'left over from before' }));
      expect(venue.ok && venue.request.location_text).toBeUndefined();
      expect(venue.ok && venue.request.venue_id).toBe(1);
    });

    it('sends the honeypot only when something filled it in', () => {
      const person = composeSubmission(form({}));
      expect(person.ok && 'website' in person.request).toBe(false);
      const bot = composeSubmission(form({ website: 'https://spam.example' }));
      expect(bot.ok && bot.request.website).toBe('https://spam.example');
    });
  });
});
