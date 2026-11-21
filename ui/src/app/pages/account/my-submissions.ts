import { Component, DestroyRef, inject, OnInit, signal, viewChild } from '@angular/core';
import { takeUntilDestroyed } from '@angular/core/rxjs-interop';
import { RouterLink } from '@angular/router';
import { catchError, switchMap, throwError } from 'rxjs';
import { MatButtonModule } from '@angular/material/button';
import { MatCardModule } from '@angular/material/card';
import { MatProgressSpinnerModule } from '@angular/material/progress-spinner';
import { ToastService } from '@honuware/ui/foundation';
import { PhotoUploadComponent } from '@honuware/ui/photos';
import {
  EVENT_SUBMISSIONS_ACCESS,
  MySubmission,
  MySubmissionDate,
  toSubmissionProblem,
} from '../../core/event-submissions-access/event-submissions-access';
import {
  FLYER_TYPE_MESSAGE,
  FlyerNotReady,
  flyerImageType,
  flyerPreparationProblem,
} from '../events/submit-event-form';
import { eventPagePath } from '../../core/utils/event-links';
import { formatEventShortDate, formatEventTimeRange } from '../../core/utils/events-date.utils';
import { videoProviderName } from '../../core/utils/video-links';
import {
  DATE_STATE_LABELS,
  rejectReasonText,
  relationshipLabel,
} from '../../core/utils/submission-labels';

// /account/submissions (EV3 §5.3): everything the signed-in member has sent
// us, newest first, each date with its state. A date that wasn't listed says
// why and carries the curator's message; a listed one links to its page.
// While any date is still waiting, the submission can be withdrawn, after a
// confirm in the page (not a browser pop-up), and it updates in place. Its
// flyer can be added, replaced or removed then too (CM3 Phase 2 §11): it goes
// on, or comes off, every waiting date. What's listed stays the curators'
// (OQ5): nothing here edits a listed event or its photo.
@Component({
  selector: 'app-my-submissions',
  imports: [
    RouterLink,
    MatButtonModule,
    MatCardModule,
    MatProgressSpinnerModule,
    PhotoUploadComponent,
  ],
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
  // The submission whose flyer picker is open, or whose flyer removal is
  // waiting on the in-page confirm. One panel at a time, page-wide.
  readonly flyerOpenId = signal<number | null>(null);
  readonly confirmingFlyerRemoveId = signal<number | null>(null);
  readonly flyerBusyId = signal<number | null>(null);
  readonly flyerError = signal('');

  // Only one picker is ever open, so there's at most one.
  private readonly flyerPicker = viewChild(PhotoUploadComponent);

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

  // What came with the date (CM3 §5.7): "With a photo · With a YouTube
  // video", or ''. Only the fact: the photo stays private until the date is
  // listed, so it isn't shown here.
  extras(date: MySubmissionDate): string {
    const extras: string[] = [];
    if (date.has_photo) {
      extras.push('With a photo');
    }
    if (date.video) {
      extras.push(`With a ${videoProviderName(date.video.provider)} video`);
    }
    return extras.join(' · ');
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

  // A waiting date has a photo: the flyer, or one a curator put on it.
  hasFlyer(submission: MySubmission): boolean {
    return submission.dates.some((date) => date.state === 'waiting' && date.has_photo);
  }

  openFlyer(submission: MySubmission): void {
    this.closePanels();
    this.flyerOpenId.set(submission.id);
  }

  cancelFlyer(): void {
    this.closePanels();
  }

  // As the submit form sends it: the same JPEG-or-PNG check, then shrunk and
  // re-encoded as a JPEG and checked against the upload limit (CM3 Phase 2
  // §13.3), before anything goes.
  attachFlyer(submission: MySubmission): void {
    const picker = this.flyerPicker();
    const flyer = picker?.pendingFile ?? null;
    const problem = !flyer
      ? 'Choose the flyer to add.'
      : !flyerImageType(flyer)
        ? FLYER_TYPE_MESSAGE
        : '';
    if (problem || !picker) {
      this.flyerError.set(problem);
      return;
    }
    this.flyerError.set('');
    this.flyerBusyId.set(submission.id);
    picker
      .preparePendingPhoto()
      .pipe(
        catchError((error: unknown) => throwError(() => new FlyerNotReady(error))),
        switchMap((prepared) => this.access.attachFlyer(submission.id, prepared!.buffer, 'jpeg')),
        takeUntilDestroyed(this.destroyRef),
      )
      .subscribe({
        next: (result) => {
          this.flyerBusyId.set(null);
          this.closePanels();
          this.setHasPhoto(submission.id, result.event_ids, true);
          this.toast.success('Your flyer is attached.');
        },
        error: (error: unknown) => {
          this.flyerBusyId.set(null);
          this.flyerError.set(
            error instanceof FlyerNotReady
              ? flyerPreparationProblem(error.reason)
              : toSubmissionProblem(error).detail,
          );
        },
      });
  }

  askToRemoveFlyer(submission: MySubmission): void {
    this.closePanels();
    this.confirmingFlyerRemoveId.set(submission.id);
  }

  cancelRemoveFlyer(): void {
    this.closePanels();
  }

  removeFlyer(submission: MySubmission): void {
    this.closePanels();
    this.flyerBusyId.set(submission.id);
    this.access
      .removeFlyer(submission.id)
      .pipe(takeUntilDestroyed(this.destroyRef))
      .subscribe({
        next: (result) => {
          this.flyerBusyId.set(null);
          this.setHasPhoto(submission.id, result.event_ids, false);
          this.toast.success('Flyer removed.');
        },
        error: (error: unknown) => {
          this.flyerBusyId.set(null);
          this.toast.error(toSubmissionProblem(error).detail);
        },
      });
  }

  askToWithdraw(submission: MySubmission): void {
    this.closePanels();
    this.confirmingId.set(submission.id);
  }

  cancelWithdraw(): void {
    this.closePanels();
  }

  withdraw(submission: MySubmission): void {
    this.closePanels();
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

  private closePanels(): void {
    this.confirmingId.set(null);
    this.flyerOpenId.set(null);
    this.confirmingFlyerRemoveId.set(null);
    this.flyerError.set('');
  }

  // The dates the server named now have (or no longer have) a photo.
  private setHasPhoto(submissionId: number, eventIds: number[], hasPhoto: boolean): void {
    const changed = new Set(eventIds);
    this.submissions.update((submissions) =>
      submissions.map((candidate) =>
        candidate.id !== submissionId
          ? candidate
          : {
              ...candidate,
              dates: candidate.dates.map((date) =>
                changed.has(date.event_id) ? { ...date, has_photo: hasPhoto } : date,
              ),
            },
      ),
    );
  }
}
