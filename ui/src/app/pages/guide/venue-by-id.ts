import { Component, inject } from '@angular/core';
import { MatButtonModule } from '@angular/material/button';
import { MatIconModule } from '@angular/material/icon';
import { CanActivateFn, Router, RouterLink } from '@angular/router';
import { catchError, map, of } from 'rxjs';
import { GUIDE_ACCESS } from '../../core/guide-access/guide-access';
import { GuidePageTitle } from './guide-page-state';

// /guide/venues/by-id/:venueId — how Stream A's event pages link to a venue's
// guide page: they know the venue's id, the guide's address uses its slug
// (GD3 OQ2, EV2 OQ3). This looks the slug up in the directory and redirects,
// so linking needed no change to either API.
//
// The lookup includes closed venues (GD2 OQ11 (c)): an old event at a venue
// that has since closed still lands on its page, which is its graveyard
// entry. A venue that isn't in the guide (or a failed lookup) gets the "not
// found" page below.
export const venueByIdRedirect: CanActivateFn = (route) => {
  const access = inject(GUIDE_ACCESS);
  const router = inject(Router);
  const venueId = Number(route.paramMap.get('venueId'));
  if (!Number.isInteger(venueId) || venueId <= 0) {
    return true;
  }
  return access.getVenues({ include_closed: true }).pipe(
    map((venues) => venues.find((venue) => venue.venue_id === venueId)),
    map((venue) => (venue ? router.createUrlTree(['/guide/venues', venue.slug]) : true)),
    catchError(() => of(true)),
  );
};

@Component({
  selector: 'app-venue-by-id-not-found',
  imports: [RouterLink, MatButtonModule, MatIconModule],
  template: `
    <a mat-button routerLink="/guide/venues" class="guide-page__back">
      <mat-icon>arrow_back</mat-icon>
      Venues
    </a>
    <h1 class="guide-page__title">Venue not found</h1>
    <p>This venue doesn't have a page in the guide yet.</p>
    <a mat-stroked-button routerLink="/guide/venues">See all venues</a>
  `,
})
export class VenueByIdNotFoundPage {
  constructor() {
    inject(GuidePageTitle).set('Venue not found');
  }
}
