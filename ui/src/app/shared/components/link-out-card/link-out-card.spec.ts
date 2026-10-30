import { TestBed } from '@angular/core/testing';
import { GuideLink, GuideLinkPlatform } from '../../../core/types/guide.types';
import { LinkOutCard } from './link-out-card';

function render(link: GuideLink): HTMLAnchorElement {
  const fixture = TestBed.createComponent(LinkOutCard);
  fixture.componentRef.setInput('link', link);
  fixture.detectChanges();
  return (fixture.nativeElement as HTMLElement).querySelector('a.link-out-card')!;
}

const LINK: GuideLink = {
  id: 1,
  label: 'Leather scene chat',
  url: 'https://example.org/leather-chat',
  platform: 'discord',
};

describe('LinkOutCard', () => {
  it('opens the link in a new tab, without handing it this page', () => {
    const anchor = render(LINK);
    expect(anchor.getAttribute('href')).toBe('https://example.org/leather-chat');
    expect(anchor.getAttribute('target')).toBe('_blank');
    expect(anchor.getAttribute('rel')).toBe('noopener noreferrer');
  });

  it('shows the label and always names the platform', () => {
    const anchor = render(LINK);
    expect(anchor.querySelector('.link-out-card__label')?.textContent?.trim()).toBe(
      'Leather scene chat',
    );
    expect(anchor.querySelector('.link-out-card__platform')?.textContent?.trim()).toBe('Discord');
  });

  it('has a name and an icon for every platform', () => {
    const platforms: GuideLinkPlatform[] = [
      'discord',
      'bluesky',
      'instagram',
      'facebook-group',
      'meetup',
      'website',
      'other',
    ];
    const names = platforms.map((platform) => {
      const anchor = render({ ...LINK, platform });
      expect(anchor.querySelector('.link-out-card__icon')?.textContent?.trim()).not.toBe('');
      return anchor.querySelector('.link-out-card__platform')?.textContent?.trim();
    });
    expect(names).toEqual([
      'Discord',
      'Bluesky',
      'Instagram',
      'Facebook group',
      'Meetup',
      'Website',
      'Link',
    ]);
  });
});
