import { Component } from '@angular/core';
import { RouterLink } from '@angular/router';
import { MatButtonModule } from '@angular/material/button';
import { MatIconModule } from '@angular/material/icon';
import { ImageUploadSettingsComponent } from '@honuware/ui/photos';

// The Site settings page (/admin/settings; CM3 Phase 2 §13.3, OQ21): settings
// for the whole site rather than the events, admins only (AdminGuard on the
// route). For now it holds @honuware/ui's upload-limit card: the largest image
// anyone may upload, 1 to 50 MB, 10 MB by default, which covers every upload
// (events, flyers, the guide, people's own photos). Future site-wide settings
// go here too.
@Component({
  selector: 'app-site-settings',
  imports: [RouterLink, MatButtonModule, MatIconModule, ImageUploadSettingsComponent],
  templateUrl: './site-settings.html',
  styleUrl: './site-settings.scss',
})
export class SiteSettingsPage {}
