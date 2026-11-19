import { TestBed } from '@angular/core/testing';
import { DomSanitizer } from '@angular/platform-browser';
import { vi } from 'vitest';
import { VideoRef } from '../../../core/types/events.types';
import { VideoEmbed } from './video-embed';

function render(video: VideoRef) {
  TestBed.configureTestingModule({ imports: [VideoEmbed] });
  const sanitizer = TestBed.inject(DomSanitizer);
  const trust = vi.spyOn(sanitizer, 'bypassSecurityTrustResourceUrl');
  const fixture = TestBed.createComponent(VideoEmbed);
  fixture.componentRef.setInput('video', video);
  fixture.componentRef.setInput('title', 'Leather Night');
  fixture.detectChanges();
  const element = fixture.nativeElement as HTMLElement;
  return { fixture, element, trust };
}

describe('VideoEmbed', () => {
  afterEach(() => vi.restoreAllMocks());

  it("shows YouTube's no-cookie player straight away: titled, lazy, fullscreen allowed, not autoplaying", () => {
    const { element } = render({ provider: 'youtube', id: 'dQw4w9WgXcQ' });
    const frame = element.querySelector('iframe')!;
    expect(frame.getAttribute('src')).toBe('https://www.youtube-nocookie.com/embed/dQw4w9WgXcQ');
    expect(frame.getAttribute('title')).toBe('Video: Leather Night');
    expect(frame.getAttribute('loading')).toBe('lazy');
    expect(frame.hasAttribute('allowfullscreen')).toBe(true);
    // No stand-in of our own any more (CM3 OQ23): the player shows its own still.
    expect(element.querySelector('button')).toBeNull();
    expect(element.querySelector('img')).toBeNull();
  });

  it("shows Vimeo's do-not-track player for a Vimeo id", () => {
    const { element } = render({ provider: 'vimeo', id: '76979871' });
    expect(element.querySelector('iframe')!.getAttribute('src')).toBe(
      'https://player.vimeo.com/video/76979871?dnt=1',
    );
  });

  it('trusts the address once, not on every change check', () => {
    const { fixture, trust } = render({ provider: 'youtube', id: 'dQw4w9WgXcQ' });
    fixture.detectChanges();
    fixture.detectChanges();
    fixture.detectChanges();
    expect(trust).toHaveBeenCalledTimes(1);
  });

  it('builds nothing from an id of the wrong shape', () => {
    const { element, trust } = render({
      provider: 'youtube',
      id: 'x" onload="alert(1)',
    });
    expect(element.querySelector('.video-embed')).toBeNull();
    expect(trust).not.toHaveBeenCalled();
  });

  it('moves the player to the new video when the page moves on', () => {
    const { fixture, element } = render({ provider: 'youtube', id: 'dQw4w9WgXcQ' });
    fixture.componentRef.setInput('video', { provider: 'vimeo', id: '76979871' });
    fixture.detectChanges();
    const frames = element.querySelectorAll('iframe');
    expect(frames.length).toBe(1);
    expect(frames[0].getAttribute('src')).toBe('https://player.vimeo.com/video/76979871?dnt=1');
  });
});
