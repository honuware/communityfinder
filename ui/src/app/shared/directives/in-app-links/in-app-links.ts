import { Directive, DOCUMENT, ElementRef, inject } from '@angular/core';
import { Router } from '@angular/router';

// For a host of rendered markdown (a guide page's body). Its links are plain
// <a href>s, which would reload the whole app; this turns a plain left-click
// on a link to somewhere in the app into a router navigation instead (GD3
// §2.6, after GD2 OQ5: bodies link to other guide pages with ordinary
// relative links).
//
// Everything else is left to the browser: other sites, mailto: and tel:,
// links with a target or download, ctrl/cmd/shift/alt- and middle-clicks
// (new tab, new window, save), same-page #anchors, and the server's own /api/
// addresses (a calendar feed, say), which aren't app pages.
@Directive({
  selector: '[appInAppLinks]',
  host: { '(click)': 'onClick($event)' },
})
export class InAppLinks {
  private host = inject<ElementRef<HTMLElement>>(ElementRef);
  private document = inject(DOCUMENT);
  private router = inject(Router);

  onClick(event: MouseEvent): void {
    if (
      event.defaultPrevented ||
      event.button !== 0 ||
      event.ctrlKey ||
      event.metaKey ||
      event.shiftKey ||
      event.altKey
    ) {
      return;
    }
    const anchor = (event.target as Element | null)?.closest?.('a');
    if (
      !anchor ||
      !this.host.nativeElement.contains(anchor) ||
      anchor.hasAttribute('download') ||
      (anchor.hasAttribute('target') && anchor.getAttribute('target') !== '_self')
    ) {
      return;
    }
    const path = this.appPath(anchor.getAttribute('href'));
    if (path === null) {
      return;
    }
    event.preventDefault();
    void this.router.navigateByUrl(path);
  }

  // The link's path within the app ("/guide/venues/x?y=1#z"), or null when it
  // isn't an app page.
  private appPath(href: string | null): string | null {
    if (!href || href.startsWith('#')) {
      return null;
    }
    let url: URL;
    try {
      url = new URL(href, this.document.baseURI);
    } catch {
      return null;
    }
    // The app's root, from <base href>: "/" here, but not assumed.
    const base = new URL(this.document.baseURI);
    const basePath = base.pathname.endsWith('/') ? base.pathname : `${base.pathname}/`;
    if (url.origin !== base.origin || !url.pathname.startsWith(basePath)) {
      return null;
    }
    const path = `/${url.pathname.slice(basePath.length)}`;
    if (path === '/api' || path.startsWith('/api/')) {
      return null;
    }
    return `${path}${url.search}${url.hash}`;
  }
}
