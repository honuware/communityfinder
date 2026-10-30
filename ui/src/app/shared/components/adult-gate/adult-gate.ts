import { NgTemplateOutlet } from '@angular/common';
import {
  Component,
  computed,
  contentChild,
  DOCUMENT,
  inject,
  input,
  signal,
  TemplateRef,
} from '@angular/core';
import { MatButtonModule } from '@angular/material/button';
import { Router } from '@angular/router';

// The answer lives in sessionStorage: it lasts until the tab closes, and it's
// never stored anywhere lasting or sent to the server (GD3 OQ1; the MHMDA-
// minded posture). Not cookies, not localStorage.
export const ADULT_OK_STORAGE_KEY = 'cf_adult_ok';

// GD3's 18+ interstitial (Q3b): a "this section is for adults" step in front
// of adult-flagged listings and pages. It is not age verification; WA has no
// age-verification law as of mid-2026.
//
// The gated body is an <ng-template>, so nothing in it is even created until
// the reader says yes; its photos don't load and its fetches don't run:
//
//   <app-adult-gate [adult]="venue.adult">
//     <ng-template>…the page…</ng-template>
//   </app-adult-gate>
//
// `adult` defaults to true. Pages pass a listing's flag, so the same wrapper
// passes non-adult content straight through.
//
// Whether adult pages should also carry a noindex tag is RT4's site-wide
// call (GD3 OQ4); nothing here sets one.
@Component({
  selector: 'app-adult-gate',
  imports: [NgTemplateOutlet, MatButtonModule],
  templateUrl: './adult-gate.html',
  styleUrl: './adult-gate.scss',
})
export class AdultGate {
  private router = inject(Router);
  private storage = sessionStorageOf(inject(DOCUMENT));

  readonly adult = input(true);
  readonly body = contentChild.required(TemplateRef);

  private readonly accepted = signal(readAccepted(this.storage));
  readonly open = computed(() => !this.adult() || this.accepted());

  accept(): void {
    try {
      this.storage?.setItem(ADULT_OK_STORAGE_KEY, 'true');
    } catch {
      // Storage is off (some private modes): the answer holds for this page.
    }
    this.accepted.set(true);
  }

  decline(): void {
    void this.router.navigateByUrl('/guide');
  }
}

function sessionStorageOf(document: Document): Storage | null {
  try {
    return document.defaultView?.sessionStorage ?? null;
  } catch {
    return null;
  }
}

function readAccepted(storage: Storage | null): boolean {
  try {
    return storage?.getItem(ADULT_OK_STORAGE_KEY) === 'true';
  } catch {
    return false;
  }
}
