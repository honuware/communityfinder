import {
  SubmissionDateState,
  SubmissionRelationship,
} from '../event-submissions-access/event-submissions-access';

// The submission pipeline's words, in one place, so the form, "My
// submissions" and the review queue say the same thing.

// "How do you know about it?"
export const RELATIONSHIPS: ReadonlyArray<{ value: SubmissionRelationship; label: string }> = [
  { value: 'organizer', label: "I'm the organizer" },
  { value: 'venue', label: 'I work at the venue' },
  { value: 'performer', label: "I'm performing" },
  { value: 'attendee', label: "I'm going" },
  { value: 'other', label: 'Something else' },
];

export function relationshipLabel(relationship: string): string {
  return RELATIONSHIPS.find((entry) => entry.value === relationship)?.label ?? 'Something else';
}

// A date's status chip on "My submissions".
export const DATE_STATE_LABELS: Record<SubmissionDateState, string> = {
  waiting: 'Waiting for review',
  listed: 'Listed',
  not_listed: 'Not listed',
  withdrawn: 'Withdrawn',
  past: 'Past',
};

// Why a submission wasn't listed, in the decision email's words
// (DescribeRejectReason in submission_email.cpp). `other` and `withdrawn`
// have none.
const REASON_TEXT: Record<string, string> = {
  duplicate: "It's already on the calendar.",
  'not-a-fit': "It isn't a fit for this calendar.",
  'missing-details': 'We needed more details to list it.',
  past: 'The date had already passed.',
  spam: "It didn't meet our listing guidelines.",
  expired: "We didn't get to it before the date passed. Sorry about that.",
};

export function rejectReasonText(reason: string): string {
  return REASON_TEXT[reason] ?? '';
}
