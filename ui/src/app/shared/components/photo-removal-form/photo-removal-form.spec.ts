import { HttpErrorResponse } from '@angular/common/http';
import { TestBed } from '@angular/core/testing';
import { Router } from '@angular/router';
import { provideNoopAnimations } from '@angular/platform-browser/animations';
import { Observable, of, throwError } from 'rxjs';
import { vi } from 'vitest';
import {
  COMMUNITY_ACCESS,
  PhotoRemovalRequest,
} from '../../../core/community-access/community-access';
import { SiteConfigService } from '../../../core/services/site-config.service';
import { PhotoRemovalForm } from './photo-removal-form';

const PAGE_URL = 'https://communityfinder.example/events/103';

function render(
  photo = 'photo 2 of 3 ("Doors at nine.")',
  reply: () => Observable<void> = () => of(undefined),
) {
  const requestPhotoRemoval = vi.fn((request: PhotoRemovalRequest) => {
    void request;
    return reply();
  });
  TestBed.configureTestingModule({
    imports: [PhotoRemovalForm],
    providers: [
      provideNoopAnimations(),
      { provide: COMMUNITY_ACCESS, useValue: { requestPhotoRemoval } },
      // Where the visitor is: the path is all that's sent.
      { provide: Router, useValue: { url: '/events/103?from=list#photos' } },
    ],
  });
  const fixture = TestBed.createComponent(PhotoRemovalForm);
  fixture.componentRef.setInput('pageTitle', 'Leather Night');
  fixture.componentRef.setInput('photo', photo);
  fixture.componentRef.setInput('pageUrl', PAGE_URL);
  const closed = vi.fn();
  fixture.componentInstance.closed.subscribe(closed);
  fixture.detectChanges();
  const element = fixture.nativeElement as HTMLElement;
  const send = () => {
    element.querySelector<HTMLButtonElement>('button[type="submit"]')!.click();
    fixture.detectChanges();
  };
  return { fixture, form: fixture.componentInstance, element, requestPhotoRemoval, closed, send };
}

function text(element: Element | null | undefined): string {
  return element?.textContent?.replace(/\s+/g, ' ').trim() ?? '';
}

function button(within: Element, label: string): HTMLButtonElement {
  return Array.from(within.querySelectorAll('button')).find((entry) => text(entry) === label)!;
}

function problem(status: number, body: Record<string, unknown>) {
  return () => throwError(() => new HttpErrorResponse({ status, error: body }));
}

describe('PhotoRemovalForm', () => {
  afterEach(() => vi.restoreAllMocks());

  it('sends the page’s path, its title, the photo and what was typed, then thanks', () => {
    const { form, element, requestPhotoRemoval, send } = render();
    expect(text(element.querySelector('h3'))).toBe('Ask us to remove this photo');
    expect(text(element.querySelector('.photo-removal__which'))).toBe(
      'About photo 2 of 3 ("Doors at nine.") on Leather Night.',
    );
    form.form.setValue({
      message: '  I’m in it, and didn’t agree to be shown.  ',
      replyEmail: ' me@example.com ',
      website: '',
    });
    send();
    expect(requestPhotoRemoval).toHaveBeenCalledWith({
      page_path: '/events/103',
      page_title: 'Leather Night',
      photo: 'photo 2 of 3 ("Doors at nine.")',
      message: 'I’m in it, and didn’t agree to be shown.',
      reply_email: 'me@example.com',
      website: '',
    });
    expect(text(element.querySelector('[role="status"]'))).toBe(
      "Thanks. Your request has gone to the people who look after this site. If the photo shouldn't be up, we'll take it down.",
    );
    expect(element.querySelector('form')).toBeNull();
  });

  it('needs nothing typed when the photo is named', () => {
    const { requestPhotoRemoval, send } = render();
    send();
    expect(requestPhotoRemoval).toHaveBeenCalledWith(
      expect.objectContaining({ message: '', reply_email: '' }),
    );
  });

  it('from the gallery, asks which photo before sending anything', () => {
    const { form, element, requestPhotoRemoval, send } = render('');
    expect(text(element.querySelector('h3'))).toBe('Ask us to remove a photo');
    expect(element.querySelector('.photo-removal__which')).toBeNull();
    send();
    expect(requestPhotoRemoval).not.toHaveBeenCalled();
    expect(text(element.querySelector('[data-field="message"]'))).toBe(
      'Tell us which photo: its number or what it shows.',
    );
    form.form.controls.message.setValue('The third one, by the bar.');
    send();
    expect(requestPhotoRemoval).toHaveBeenCalledWith(
      expect.objectContaining({ photo: '', message: 'The third one, by the bar.' }),
    );
  });

  it('checks the reply address before sending', () => {
    const { form, element, requestPhotoRemoval, send } = render();
    form.form.controls.replyEmail.setValue('not an address');
    send();
    expect(requestPhotoRemoval).not.toHaveBeenCalled();
    expect(text(element.querySelector('[data-field="reply_email"]'))).toBe(
      "That doesn't look like an email address.",
    );
  });

  it('shows a refusal about a field by that field, keeping what was typed', () => {
    const { form, element, send } = render(
      'photo 1 of 1',
      problem(400, {
        error_code: 'TOO_LONG',
        field: 'message',
        detail: 'Please keep your message under 2000 characters.',
      }),
    );
    form.form.controls.message.setValue('Long enough for the server to refuse.');
    send();
    expect(text(element.querySelector('[data-field="message"]'))).toBe(
      'Please keep your message under 2000 characters.',
    );
    expect(form.form.controls.message.value).toBe('Long enough for the server to refuse.');
  });

  it('offers the contact address, with the request written out, when it can’t be sent', () => {
    const { fixture, form, element, send } = render(
      'photo 2 of 3 ("Doors at nine.")',
      problem(503, { error_code: 'NOT_SENT', detail: 'x' }),
    );
    form.form.controls.message.setValue('Please take it down.');
    send();
    const alert = element.querySelector('[role="alert"]')!;
    const contact = TestBed.inject(SiteConfigService).config.contactEmail;
    expect(text(alert)).toBe(
      `We couldn't send your request just now. Please email us at ${contact} instead.`,
    );
    const mailto = new URL(alert.querySelector('a')!.getAttribute('href')!);
    expect(mailto.protocol).toBe('mailto:');
    expect(mailto.pathname).toBe(contact);
    expect(mailto.searchParams.get('subject')).toBe('Photo removal: Leather Night');
    expect(mailto.searchParams.get('body')).toContain(
      `Please remove photo 2 of 3 ("Doors at nine.") on ${PAGE_URL}.`,
    );

    button(element, 'Try again').click();
    fixture.detectChanges();
    expect(form.state()).toBe('open');
    expect(element.querySelector('form')).not.toBeNull();
    expect(form.form.controls.message.value).toBe('Please take it down.');
  });

  it('says so when the visitor has sent too many this hour', () => {
    const { element, send } = render(
      'photo 1 of 1',
      problem(429, { error_code: 'RATE_LIMITED', detail: 'x' }),
    );
    send();
    expect(text(element.querySelector('[role="alert"]'))).toContain(
      "You've sent several requests in the last hour. Please email us at",
    );
  });

  it('passes a filled-in honeypot along, for the server to drop', () => {
    const { form, requestPhotoRemoval, send } = render();
    form.form.controls.website.setValue('http://spam.example');
    send();
    expect(requestPhotoRemoval).toHaveBeenCalledWith(
      expect.objectContaining({ website: 'http://spam.example' }),
    );
  });

  it('keeps the honeypot away from people and the keyboard', () => {
    const { element } = render();
    const trap = element.querySelector('.photo-removal__trap')!;
    expect(trap.getAttribute('aria-hidden')).toBe('true');
    expect(trap.querySelector('input')!.getAttribute('tabindex')).toBe('-1');
  });

  it('Cancel and Close say it’s done', () => {
    const { element, closed, send } = render();
    button(element, 'Cancel').click();
    expect(closed).toHaveBeenCalledTimes(1);
    send();
    button(element, 'Close').click();
    expect(closed).toHaveBeenCalledTimes(2);
  });
});
