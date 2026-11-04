import {
  datePickerValue,
  pickedDateText,
  pickedTimeText,
  timePickerValue,
} from './date-picker-values';

describe('date picker values', () => {
  it('reads the local calendar day of a picked date', () => {
    expect(pickedDateText(new Date(2026, 9, 31))).toBe('2026-10-31');
    expect(pickedDateText(new Date(2026, 2, 8, 23, 30))).toBe('2026-03-08');
  });

  it('reads the local clock time of a picked time', () => {
    expect(pickedTimeText(new Date(2026, 0, 1, 22, 0))).toBe('22:00');
    expect(pickedTimeText(new Date(2026, 0, 1, 9, 5))).toBe('09:05');
  });

  it('reads nothing from an empty or unreadable picker', () => {
    for (const value of [null, undefined, '', new Date(NaN), '2026-10-31']) {
      expect(pickedDateText(value)).toBe('');
      expect(pickedTimeText(value)).toBe('');
    }
  });

  it('gives the pickers back the same day and time', () => {
    expect(pickedDateText(new Date(datePickerValue('2026-10-31')))).toBe('2026-10-31');
    expect(pickedDateText(new Date(datePickerValue('2026-03-08')))).toBe('2026-03-08');
    expect(pickedTimeText(new Date(timePickerValue('22:00')))).toBe('22:00');
    expect(pickedTimeText(new Date(timePickerValue('2:30')))).toBe('02:30');
  });

  it('gives the pickers nothing for blank or malformed text', () => {
    for (const text of ['', 'soon', '10/31/2026']) {
      expect(datePickerValue(text)).toBe('');
    }
    for (const text of ['', '10 PM', '24:00', '9:60']) {
      expect(timePickerValue(text)).toBe('');
    }
  });
});
