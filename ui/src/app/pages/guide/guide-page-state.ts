import { HttpErrorResponse } from '@angular/common/http';
import { effect, inject, Injectable, Signal } from '@angular/core';
import { toSignal } from '@angular/core/rxjs-interop';
import { Title } from '@angular/platform-browser';
import { ActivatedRoute, NavigationStart, Router } from '@angular/router';
import { catchError, filter, map, Observable, of, startWith, switchMap, throwError } from 'rxjs';
import {
  EMPTY_GUIDE_VOCABULARIES,
  GuideVocabularies,
  GuideVocabulariesService,
} from '../../core/services/guide-vocabularies.service';
import { SiteConfigService } from '../../core/services/site-config.service';

// One page's data and where its load stands: the knottyyoga detail-page
// pattern's loading / notFound / failed flags (GD3 §3.1). A 404 is "not
// found"; any other failure is "failed".
export interface GuideLoad<T> {
  loading: boolean;
  notFound: boolean;
  failed: boolean;
  data: T | null;
}

const LOADING: GuideLoad<never> = { loading: true, notFound: false, failed: false, data: null };
const NOT_FOUND: GuideLoad<never> = { loading: false, notFound: true, failed: false, data: null };
const FAILED: GuideLoad<never> = { loading: false, notFound: false, failed: true, data: null };

function loaded<T>(data: T): GuideLoad<T> {
  return { loading: false, notFound: false, failed: false, data };
}

// Loads afresh each time `params` emits (a route parameter or the query): the
// component stays put from /guide/venues/a to /guide/venues/b, so a new slug
// is a new load. Call it in a field initializer.
export function loadEach<P, T>(
  params: Observable<P>,
  fetch: (params: P) => Observable<T>,
): Signal<GuideLoad<T>> {
  return toSignal(
    params.pipe(
      switchMap((value) =>
        fetch(value).pipe(
          map((data) => loaded(data)),
          catchError((error: unknown) =>
            of(error instanceof HttpErrorResponse && error.status === 404 ? NOT_FOUND : FAILED),
          ),
          startWith(LOADING),
        ),
      ),
    ),
    { initialValue: LOADING },
  );
}

// A route parameter as it changes ('' when absent). Call it in a field
// initializer.
export function routeParam(name: string): Observable<string> {
  return inject(ActivatedRoute).paramMap.pipe(map((params) => params.get(name) ?? ''));
}

// The scene and neighborhood names for a page's chips and cards; empty until
// they're in (chips show slugs meanwhile). Call it in a field initializer.
export function guideVocabularies(): Signal<GuideVocabularies> {
  return toSignal(inject(GuideVocabulariesService).vocabularies$, {
    initialValue: EMPTY_GUIDE_VOCABULARIES,
  });
}

// For a page that knows without asking the server that there's nothing to
// show (an unknown section): loads to "not found".
export function notFound<T>(): Observable<T> {
  return throwError(() => new HttpErrorResponse({ status: 404, statusText: 'Not Found' }));
}

// The browser-tab title, "{page} · {site name}", the same shape as the events
// pages'. Site-wide metadata (descriptions, social cards) is RT4's, so the
// guide sets the title only. Every navigation starts from the bare site name,
// so a page that sets nothing never keeps the last page's title.
@Injectable({ providedIn: 'root' })
export class GuidePageTitle {
  private title = inject(Title);
  private siteConfig = inject(SiteConfigService);

  constructor() {
    inject(Router)
      .events.pipe(filter((event) => event instanceof NavigationStart))
      .subscribe(() => this.title.setTitle(this.siteConfig.config.displayName));
  }

  set(pageTitle: string): void {
    this.title.setTitle(`${pageTitle} · ${this.siteConfig.config.displayName}`);
  }
}

// Keeps the tab title in step with a page's load: the page's own title once
// it's in, `notFoundTitle` on a 404. Call it in a constructor or field
// initializer.
export function titleFromLoad<T>(
  load: Signal<GuideLoad<T>>,
  titleOf: (data: T) => string,
  notFoundTitle: string,
): void {
  const pageTitle = inject(GuidePageTitle);
  effect(() => {
    const current = load();
    if (current.data) {
      pageTitle.set(titleOf(current.data));
    } else if (current.notFound) {
      pageTitle.set(notFoundTitle);
    }
  });
}
