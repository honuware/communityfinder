import { ColumnDataInfo } from '@honuware/ui/access';

// The honuware date and time pickers (hw-simple-date) hold a browser-local
// Date. Our forms hold Seattle wall-clock text ("2026-08-14", "22:00"), so
// these read and write the picker's local calendar date and clock time: what
// the picker shows is what the form holds, whatever the browser's time zone.

// What a picker needs to know about its field: the label (it adds " date" or
// " time") and whether the field is required.
export function pickerField(label: string, required: boolean): ColumnDataInfo {
  return {
    column_name: label.toLowerCase(),
    type: 'TEXT',
    primary_key: false,
    unique: false,
    nullable: !required,
    label,
    required,
  };
}

function validDate(value: unknown): Date | null {
  return value instanceof Date && !isNaN(value.getTime()) ? value : null;
}

function twoDigits(value: number): string {
  return String(value).padStart(2, '0');
}

// A picked date → "2026-08-14"; blank for none, or for typing that didn't parse.
export function pickedDateText(value: unknown): string {
  const date = validDate(value);
  return date
    ? `${date.getFullYear()}-${twoDigits(date.getMonth() + 1)}-${twoDigits(date.getDate())}`
    : '';
}

// A picked time → "22:00"; blank for none, or for typing that didn't parse.
export function pickedTimeText(value: unknown): string {
  const date = validDate(value);
  return date ? `${twoDigits(date.getHours())}:${twoDigits(date.getMinutes())}` : '';
}

// "2026-08-14" → the value the date picker takes to show that day; blank for
// blank or malformed text.
export function datePickerValue(text: string): string {
  const match = /^(\d{4})-(\d{2})-(\d{2})$/.exec(text.trim());
  if (!match) {
    return '';
  }
  const [year, month, day] = match.slice(1).map(Number);
  return new Date(year, month - 1, day).toISOString();
}

// "22:00" → the value the time picker takes to show that time; blank for
// blank or malformed text. The day under it is a fixed winter one, so no
// daylight-saving jump moves the clock time.
export function timePickerValue(text: string): string {
  const match = /^(\d{1,2}):(\d{2})$/.exec(text.trim());
  if (!match) {
    return '';
  }
  const [hour, minute] = match.slice(1).map(Number);
  return hour > 23 || minute > 59 ? '' : new Date(2000, 0, 1, hour, minute).toISOString();
}
