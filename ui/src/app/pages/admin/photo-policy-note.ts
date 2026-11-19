import { Component } from '@angular/core';

// CM3's photo policy (consent and no outing), shown beside every upload in
// the curator's editor: the main photo's and each gallery photo's.
@Component({
  selector: 'app-photo-policy-note',
  template: `
    <details class="photo-policy-note">
      <summary>Which photos are fine</summary>
      <ul>
        <li>
          <strong>Always fine:</strong> flyers and artwork, the venue, performers' own promo images,
          and crowd shots where nobody can be recognized.
        </li>
        <li>
          <strong>A recognizable attendee</strong> only with their agreement. Performers on stage
          are fine.
        </li>
        <li>
          <strong>Captions and alt text</strong> never name attendees. Name performers or organizers
          only with their agreement.
        </li>
        <li><strong>Adult events</strong> get no photos.</li>
        <li>If someone asks for a photo to come down, take it down within a day, no questions.</li>
      </ul>
    </details>
  `,
  styles: `
    .photo-policy-note {
      margin: 4px 0 12px;
      font-size: 0.85rem;
    }
    summary {
      cursor: pointer;
      color: var(--mat-sys-primary);
    }
    ul {
      margin: 6px 0 0;
      padding-left: 20px;
    }
  `,
})
export class PhotoPolicyNote {}
