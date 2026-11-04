import { Component, DestroyRef, inject, OnInit, signal } from '@angular/core';
import { takeUntilDestroyed } from '@angular/core/rxjs-interop';
import { RouterLink } from '@angular/router';
import { MatButtonModule } from '@angular/material/button';
import { MatCardModule } from '@angular/material/card';
import { MatProgressSpinnerModule } from '@angular/material/progress-spinner';
import { ToastService } from '@honuware/ui/foundation';
import {
  EVENT_SUBMISSIONS_ACCESS,
  MySubmission,
  MySubmissionDate,
  toSubmissionProblem,
} from '../../core/event-submissions-access/event-submissions-access';
import { eventPagePath } from '../../core/utils/event-links';
import { formatEventShortDate, formatEventTimeRange } from '../../core/utils/events-date.utils';
import {
  DATE_STATE_LABELS,
  rejectReasonText,
  relationshipLabel,
} from '../../core/utils/submission-labels';

// /account/submissions (EV3 §5.3): everything the signed-in member has sent
// us, newest first, each date with its state. A date that wasn't listed says
// why and carries the curator's message; a listed one links to its page.
// While any date is still waiting, the submission can be withdrawn, after a
// confirm in the page (not a browser pop-up), and it updates in place. What's
// listed stays the curators' (OQ5): nothing here edits an event.
@Component({
  selector: 'app-my-submissions',
  imports: [RouterLink, MatButtonModule, MatCardModule, MatProgressSpinnerModule],
  templateUrl: './my-submissions.html',
  styleUrl: './my-submissions.scss',
})
export class MySubmissionsPage implements OnInit {
  private access = inject(EVENT_SUBMISSIONS_ACCESS);
  private toast = inject(ToastService);
  private destroyRef = inject(DestroyRef);

  readonly loading = signal(true);
  readonly failed = signal(false);
  readonly submissions = signal<MySubmission[]>([]);
  // The submission whose withdraw is waiting on the in-page confirm.
  readonly confirmingId = signal<number | null>(null);
  readonly withdrawingId = signal<number | null>(null);

  ngOnInit(): void {
    this.access
      .getMine()
      .pipe(takeUntilDestroyed(this.destroyRef))
      .subscribe({
        next: (submissions) => {
          this.submissions.set(submissions);
          this.loading.set(false);
        },
        error: () => {
          this.failed.set(true);
          this.loading.set(false);
        },
      });
  }

  // "Leather Night", or "Leather Night (3 dates)".
  heading(submission: MySubmission): string {
    const title = submission.dates[0]?.title ?? `Submission ${submission.id}`;
    return submission.dates.length > 1 ? `${title} (${submission.dates.length} dates)` : title;
  }

  sentOn(submission: MySubmission): string {
    return formatEventShortDate(submission.created_us);
  }

  relationship(submission: MySubmission): string {
    return relationshipLabel(submission.relationship);
  }

  when(date: MySubmissionDate): string {
    return `${formatEventShortDate(date.starts_at_us)} · ${formatEventTimeRange(
      date.starts_at_us,
      date.ends_at_us,
    )}`;
  }

  stateLabel(date: MySubmissionDate): string {
    return DATE_STATE_LABELS[date.state] ?? date.state;
  }

  eventPath(date: MySubmissionDate): string {
    return eventPagePath(date.event_id);
  }

  // The reason, when a curator turned at least one date down.
  reason(submission: MySubmission): string {
    return submission.dates.some((date) => date.state === 'not_listed' || date.state === 'past')
      ? rejectReasonText(submission.reject_reason)
      : '';
  }

  canWithdraw(submission: MySubmission): boolean {
    return submission.dates.some((date) => date.state === 'waiting');
  }

  waitingCount(submission: MySubmission): number {
    return submission.dates.filter((date) => date.state === 'waiting').length;
  }

  askToWithdraw(submission: MySubmission): void {
    this.confirmingId.set(submission.id);
  }

  cancelWithdraw(): void {
    this.confirmingId.set(null);
  }

  withdraw(submission: MySubmission): void {
    this.confirmingId.set(null);
    this.withdrawingId.set(submission.id);
    this.access
      .withdraw(submission.id)
      .pipe(takeUntilDestroyed(this.destroyRef))
      .subscribe({
        next: (result) => {
          this.withdrawingId.set(null);
          const withdrawn = new Set(result.withdrawn_event_ids);
          this.submissions.update((submissions) =>
            submissions.map((candidate) =>
              candidate.id !== submission.id
                ? candidate
                : {
                    ...candidate,
                    dates: candidate.dates.map((date) =>
                      withdrawn.has(date.event_id) ? { ...date, state: 'withdrawn' } : date,
                    ),
                  },
            ),
          );
          this.toast.success('Withdrawn. Those dates are out of the queue.');
        },
        error: (error: unknown) => {
          this.withdrawingId.set(null);
          this.toast.error(toSubmissionProblem(error).detail);
        },
      });
  }
}
