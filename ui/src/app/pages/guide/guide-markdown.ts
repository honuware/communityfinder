import { Component, input, ViewEncapsulation } from '@angular/core';
import { MarkdownComponent } from 'ngx-markdown';
import { InAppLinks } from '../../shared/directives/in-app-links/in-app-links';

// A guide page's body: markdown, rendered by ngx-markdown with its default
// sanitizing on (GD3's Markdown pin; bodies may carry plain HTML, filtered,
// per GD2 OQ5), with the in-app-links directive so a link to another guide
// page navigates without reloading the app. Articles, scene intros and
// neighborhood rundowns all render through this.
//
// No view encapsulation: the body's HTML is made at runtime, so scoped styles
// couldn't reach it. Every rule here sits under .guide-markdown.
@Component({
  selector: 'app-guide-markdown',
  imports: [MarkdownComponent, InAppLinks],
  template: '<markdown class="guide-markdown" appInAppLinks [data]="body()" />',
  styleUrl: './guide-markdown.scss',
  encapsulation: ViewEncapsulation.None,
})
export class GuideMarkdown {
  readonly body = input.required<string>();
}
