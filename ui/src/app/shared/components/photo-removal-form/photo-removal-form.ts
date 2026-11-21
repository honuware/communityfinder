import {
  afterNextRender,
  Component,
  computed,
  DestroyRef,
  ElementRef,
  inject,
  Injector,
  input,
  output,
  signal,
  viewChild,
} from '@angular/core';
import { takeUntilDestroyed } from '@angular/core/rxjs-interop';
import { HttpErrorResponse } from '@angular/common/http';
import { FormControl, FormGroup, ReactiveFormsModule } from '@angular/forms';
import { Router } from '@angular/router';
import { MatButtonModule } from '@angular/material/button';
import { MatFormFieldModule } from '@angular/material/form-field';
import { MatInputModule } from '@angular/material/input';
import {
  COMMUNITY_ACCESS,
  PHOTO_REMOVAL_EMAIL_MAX,
  PHOTO_REMOVAL_MESSAGE_MAX,
  PHOTO_REMOVAL_RATE_LIMITED,
} from '../../../core/community-access/community-access';
import { SiteConfigService } from '../../../core/services/site-config.service';

export type PhotoRemovalState = 'open' | 'sending' | 'sent' | 'failed';

// The fields the visitor can fix; any other refusal falls back to email.
type FixableField = 'message' | 'reply_email';

interface FieldProblem {
  field: FixableField;
  message: string;
}

// The server's messages (CheckPhotoRemovalRequest), checked here first.
export const PHOTO_REMOVAL_WHICH_MESSAGE = 'Tell us which photo: its number or what it shows.';
const MESSAGE_TOO_LONG = `Please keep your message under ${PHOTO_REMOVAL_MESSAGE_MAX} characters.`;
const EMAIL_TOO_LONG = 'That email address is too long.';
const EMAIL_BAD = "That doesn't look like an email address.";
const NOT_SENT = "We couldn't send your request just now.";
const RATE_LIMITED = "You've sent several requests in the last hour.";

let nextId = 0;

// CM3 Phase 2 §12 (OQ11, OQ19): "Ask us to remove a photo", as a short form in
// the page instead of a mailto. It goes to POST /api/photo_removal_requests,
// which emails the site and everyone holding manage_events. Which page comes
// from where the visitor is; which photo from the viewer (or the visitor's
// message, from the gallery). A message and a reply address are optional,
// except that from the gallery the message has to say which photo. The
// website field is the honeypot, hidden from people. When sending fails the
// form says so and offers the site's contact address, with the request
// written out, as the fallback (SiteConfig.contactEmail).
@Component({
  selector: 'app-photo-removal-form',
  imports: [ReactiveFormsModule, MatButtonModule, MatFormFieldModule, MatInputModule],
  templateUrl: './photo-removal-form.html',
  styleUrl: './photo-removal-form.scss',
})
export class PhotoRemovalForm {
  private access = inject(COMMUNITY_ACCESS);
  private siteConfig = inject(SiteConfigService);
  private router = inject(Router);
  private destroyRef = inject(DestroyRef);
  private injector = inject(Injector);

  // What the page is of ("Leather Night").
  readonly pageTitle = input.required<string>();
  // Which photo ('photo 2 of 4 ("Midnight on the floor.")'), or '' to let the
  // message say.
  readonly photo = input('');
  // The page's absolute address, for the fallback email.
  readonly pageUrl = input('');
  readonly closed = output<void>();

  readonly state = signal<PhotoRemovalState>('open');
  readonly failure = signal('');
  readonly problem = signal<FieldProblem | null>(null);
  readonly headingId = `photo-removal-heading-${++nextId}`;

  readonly form = new FormGroup({
    message: new FormControl('', { nonNullable: true }),
    replyEmail: new FormControl('', { nonNullable: true }),
    website: new FormControl('', { nonNullable: true }),
  });

  readonly heading = computed(() =>
    this.photo() ? 'Ask us to remove this photo' : 'Ask us to remove a photo',
  );

  // The same request, written out for the visitor's own email.
  readonly mailtoHref = computed(() => {
    const which = this.photo();
    const page = this.pageUrl() ? ` on ${this.pageUrl()}` : '';
    const body = which
      ? `Please remove ${which}${page}.\n\n`
      : `Please remove a photo${page}.\n\nWhich one (its number or caption):\n\n`;
    return (
      `mailto:${this.contactEmail}` +
      `?subject=${encodeURIComponent(`Photo removal: ${this.pageTitle()}`)}` +
      `&body=${encodeURIComponent(body)}`
    );
  });

  private readonly messageField = viewChild<ElementRef<HTMLTextAreaElement>>('messageField');

  constructor() {
    afterNextRender(() => this.messageField()?.nativeElement.focus(), { injector: this.injector });
  }

  // Getters, not fields: the unit-test transform can leave an imported
  // constant used as a whole field initializer undefined.
  get messageMax(): number {
    return PHOTO_REMOVAL_MESSAGE_MAX;
  }

  get emailMax(): number {
    return PHOTO_REMOVAL_EMAIL_MAX;
  }

  get contactEmail(): string {
    return this.siteConfig.config.contactEmail;
  }

  errorFor(field: FixableField): string {
    const problem = this.problem();
    return problem?.field === field ? problem.message : '';
  }

  send(): void {
    if (this.state() === 'sending') {
      return;
    }
    const value = this.form.getRawValue();
    const message = value.message.trim();
    const replyEmail = value.replyEmail.trim();
    const problem = this.check(message, replyEmail);
    this.problem.set(problem);
    if (problem) {
      return;
    }
    this.state.set('sending');
    this.access
      .requestPhotoRemoval({
        page_path: this.pagePath(),
        page_title: this.pageTitle(),
        photo: this.photo(),
        message,
        reply_email: replyEmail,
        website: value.website,
      })
      .pipe(takeUntilDestroyed(this.destroyRef))
      .subscribe({
        next: () => this.state.set('sent'),
        error: (error: unknown) => this.onRefused(error),
      });
  }

  // Back to the form, with what was typed.
  tryAgain(): void {
    this.failure.set('');
    this.state.set('open');
  }

  close(): void {
    this.closed.emit();
  }

  private check(message: string, replyEmail: string): FieldProblem | null {
    if (utf8Length(message) > PHOTO_REMOVAL_MESSAGE_MAX) {
      return { field: 'message', message: MESSAGE_TOO_LONG };
    }
    if (!this.photo() && !message) {
      return { field: 'message', message: PHOTO_REMOVAL_WHICH_MESSAGE };
    }
    if (utf8Length(replyEmail) > PHOTO_REMOVAL_EMAIL_MAX) {
      return { field: 'reply_email', message: EMAIL_TOO_LONG };
    }
    if (replyEmail && !looksLikeEmail(replyEmail)) {
      return { field: 'reply_email', message: EMAIL_BAD };
    }
    return null;
  }

  // A refusal about something the visitor typed goes by that field; anything
  // else (the limit, nothing sent, the network) offers the email instead.
  private onRefused(error: unknown): void {
    const body =
      error instanceof HttpErrorResponse && error.error && typeof error.error === 'object'
        ? (error.error as Record<string, unknown>)
        : {};
    const field = body['field'];
    const detail = typeof body['detail'] === 'string' ? body['detail'] : '';
    if (
      error instanceof HttpErrorResponse &&
      error.status === 400 &&
      (field === 'message' || field === 'reply_email') &&
      detail
    ) {
      this.problem.set({ field, message: detail });
      this.state.set('open');
      return;
    }
    const rateLimited =
      (error instanceof HttpErrorResponse && error.status === 429) ||
      body['error_code'] === PHOTO_REMOVAL_RATE_LIMITED;
    this.failure.set(rateLimited ? RATE_LIMITED : NOT_SENT);
    this.state.set('failed');
  }

  // Where the visitor is on this site: the path alone, no query or fragment.
  private pagePath(): string {
    const path = this.router.url.split(/[?#]/)[0];
    return path.startsWith('/') ? path : `/${path}`;
  }
}

function utf8Length(text: string): number {
  return new TextEncoder().encode(text).length;
}

// The server's rule: one @, something before it, a dotted domain, no spaces.
function looksLikeEmail(text: string): boolean {
  return /^[^\s@]+@[^\s@.]+(\.[^\s@.]+)+$/.test(text);
}
