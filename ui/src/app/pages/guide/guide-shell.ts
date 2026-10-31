import { Component, ViewEncapsulation } from '@angular/core';
import { RouterOutlet } from '@angular/router';

// The frame every /guide page renders in. It carries the guide pages' shared
// styles (the .guide-page classes), loaded once with the guide's lazy chunk
// rather than copied into each page's own stylesheet. No view encapsulation,
// so the pages inside can use them; every rule sits under .guide-page.
@Component({
  selector: 'app-guide-shell',
  imports: [RouterOutlet],
  template: '<div class="guide-page"><router-outlet /></div>',
  styleUrl: './guide-shell.scss',
  encapsulation: ViewEncapsulation.None,
})
export class GuideShell {}
