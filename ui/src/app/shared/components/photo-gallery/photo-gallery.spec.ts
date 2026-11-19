import { TestBed } from '@angular/core/testing';
import { GalleryPhoto } from '../../../core/types/events.types';
import { SiteConfigService } from '../../../core/services/site-config.service';
import { PhotoGallery } from './photo-gallery';

const PHOTOS: GalleryPhoto[] = [
  {
    id: 7,
    alt_text: 'The bar lit in red',
    caption: 'The main room before doors.',
    credit: 'Photo: Lantern Room',
  },
  { id: 3, alt_text: 'The flyer', caption: '', credit: 'Flyer art: the organizers' },
  { id: 9, alt_text: 'A crowded dance floor', caption: '', credit: '' },
];

function render(photos: GalleryPhoto[] = PHOTOS) {
  TestBed.configureTestingModule({ imports: [PhotoGallery] });
  const fixture = TestBed.createComponent(PhotoGallery);
  fixture.componentRef.setInput('photos', photos);
  fixture.componentRef.setInput('title', 'Leather Night');
  fixture.componentRef.setInput('pageUrl', 'https://communityfinder.example/events/103');
  fixture.detectChanges();
  // In the document (TestBed's root element), so focus can be checked.
  const element = fixture.nativeElement as HTMLElement;
  const thumbs = () =>
    Array.from(element.querySelectorAll<HTMLButtonElement>('.photo-gallery__thumb'));
  const viewer = () => element.querySelector<HTMLElement>('[role="dialog"]');
  const openAt = async (index: number) => {
    thumbs()[index].focus();
    thumbs()[index].click();
    fixture.detectChanges();
    await fixture.whenStable();
  };
  const key = (name: string) => {
    document.dispatchEvent(new KeyboardEvent('keydown', { key: name, bubbles: true }));
    fixture.detectChanges();
  };
  return { fixture, element, thumbs, viewer, openAt, key };
}

function text(element: Element | null | undefined): string {
  return element?.textContent?.replace(/\s+/g, ' ').trim() ?? '';
}

function swipe(target: HTMLElement, fromX: number, toX: number, fromY = 100, toY = 100): void {
  const touch = (x: number, y: number) => ({ clientX: x, clientY: y }) as Touch;
  const start = new Event('touchstart', { bubbles: true });
  Object.defineProperty(start, 'changedTouches', { value: [touch(fromX, fromY)] });
  const end = new Event('touchend', { bubbles: true });
  Object.defineProperty(end, 'changedTouches', { value: [touch(toX, toY)] });
  target.dispatchEvent(start);
  target.dispatchEvent(end);
}

describe('PhotoGallery', () => {
  it('shows a grid of thumbnails in gallery order, each named by its alt text', () => {
    const { thumbs } = render();
    const images = thumbs().map((thumb) => thumb.querySelector('img')!);
    expect(images.map((image) => image.getAttribute('src'))).toEqual([
      '/api/get_scaled_photo/event_gallery_photos/7/480/360',
      '/api/get_scaled_photo/event_gallery_photos/3/480/360',
      '/api/get_scaled_photo/event_gallery_photos/9/480/360',
    ]);
    expect(images.map((image) => image.getAttribute('alt'))).toEqual([
      'The bar lit in red',
      'The flyer',
      'A crowded dance floor',
    ]);
  });

  it('renders nothing for an empty gallery', () => {
    const { element } = render([]);
    expect(element.querySelector('.photo-gallery')).toBeNull();
  });

  it('opens a photo full size with its alt text, caption and credit, focus on Close', async () => {
    const { element, viewer, openAt } = render();
    expect(viewer()).toBeNull();
    await openAt(0);
    const dialog = viewer()!;
    expect(dialog.getAttribute('aria-modal')).toBe('true');
    expect(dialog.getAttribute('aria-label')).toBe('Photo 1 of 3');
    const full = dialog.querySelector('.photo-gallery__full')!;
    expect(full.getAttribute('src')).toBe('/api/get_scaled_photo/event_gallery_photos/7/1600/1200');
    expect(full.getAttribute('alt')).toBe('The bar lit in red');
    expect(text(dialog.querySelector('.photo-gallery__caption-text'))).toBe(
      'The main room before doors.',
    );
    expect(text(dialog.querySelector('.photo-gallery__credit'))).toBe('Photo: Lantern Room');
    expect(document.activeElement).toBe(element.querySelector('.photo-gallery__close'));
  });

  it('leaves out the caption line when a photo has neither', async () => {
    const { viewer, openAt } = render();
    await openAt(2);
    expect(viewer()!.querySelector('figcaption')).toBeNull();
  });

  it('moves with the arrow keys and the buttons, wrapping at the ends', async () => {
    const { fixture, viewer, openAt, key } = render();
    const shownId = () =>
      viewer()!.querySelector('.photo-gallery__full')!.getAttribute('src')!.split('/')[4];
    await openAt(0);
    key('ArrowRight');
    expect(shownId()).toBe('3');
    key('ArrowRight');
    key('ArrowRight');
    expect(shownId()).toBe('7');
    key('ArrowLeft');
    expect(shownId()).toBe('9');
    expect(text(viewer()!.querySelector('.photo-gallery__position'))).toBe('3 of 3');
    viewer()!.querySelector<HTMLButtonElement>('.photo-gallery__step--next')!.click();
    fixture.detectChanges();
    expect(shownId()).toBe('7');
    viewer()!.querySelector<HTMLButtonElement>('.photo-gallery__step--previous')!.click();
    fixture.detectChanges();
    expect(shownId()).toBe('9');
  });

  it('closes on Escape and returns focus to the thumbnail it opened from', async () => {
    const { thumbs, viewer, openAt, key } = render();
    await openAt(1);
    key('ArrowRight');
    key('Escape');
    expect(viewer()).toBeNull();
    expect(document.activeElement).toBe(thumbs()[1]);
  });

  it('closes from the Close button and the backdrop, but not from the photo', async () => {
    const { fixture, thumbs, viewer, openAt } = render();
    await openAt(0);
    viewer()!.querySelector<HTMLElement>('.photo-gallery__full')!.click();
    fixture.detectChanges();
    expect(viewer()).not.toBeNull();
    viewer()!.click();
    fixture.detectChanges();
    expect(viewer()).toBeNull();
    expect(document.activeElement).toBe(thumbs()[0]);

    await openAt(2);
    viewer()!.querySelector<HTMLButtonElement>('.photo-gallery__close')!.click();
    fixture.detectChanges();
    expect(viewer()).toBeNull();
    expect(document.activeElement).toBe(thumbs()[2]);
  });

  it('moves on a sideways swipe, and not on a mostly-vertical one', async () => {
    const { fixture, viewer, openAt } = render();
    const position = () => text(viewer()!.querySelector('.photo-gallery__position'));
    await openAt(0);
    swipe(viewer()!, 300, 200);
    fixture.detectChanges();
    expect(position()).toBe('2 of 3');
    swipe(viewer()!, 200, 300);
    fixture.detectChanges();
    expect(position()).toBe('1 of 3');
    swipe(viewer()!, 200, 150, 100, 300);
    fixture.detectChanges();
    expect(position()).toBe('1 of 3');
  });

  it('has no steppers for a single photo', async () => {
    const { viewer, openAt, key } = render([PHOTOS[0]]);
    await openAt(0);
    expect(viewer()!.querySelector('.photo-gallery__step')).toBeNull();
    key('ArrowRight');
    expect(text(viewer()!.querySelector('.photo-gallery__position'))).toBe('1 of 1');
  });

  it('asks for a removal at the site’s contact address, naming the page and the photo', async () => {
    const { element, viewer, openAt } = render();
    const contact = TestBed.inject(SiteConfigService).config.contactEmail;
    const general = new URL(
      element.querySelector('.photo-gallery__removal a')!.getAttribute('href')!,
    );
    expect(text(element.querySelector('.photo-gallery__removal a'))).toBe(
      'Ask us to remove a photo',
    );
    expect(general.protocol).toBe('mailto:');
    expect(general.pathname).toBe(contact);
    expect(general.searchParams.get('subject')).toBe('Photo removal: Leather Night');
    expect(general.searchParams.get('body')).toContain(
      'Please remove a photo on https://communityfinder.example/events/103.',
    );

    await openAt(0);
    const one = new URL(
      viewer()!.querySelector('.photo-gallery__removal-link')!.getAttribute('href')!,
    );
    expect(one.searchParams.get('body')).toContain(
      'Please remove photo 1 of 3 ("The main room before doors.") on https://communityfinder.example/events/103.',
    );
  });

  it('closes when the page moves on to another gallery', async () => {
    const { fixture, viewer, openAt } = render();
    await openAt(0);
    fixture.componentRef.setInput('photos', [PHOTOS[1]]);
    fixture.detectChanges();
    expect(viewer()).toBeNull();
  });
});
