import { TestBed } from '@angular/core/testing';
import { provideRouter } from '@angular/router';
import { GuideScene } from '../../../core/types/guide.types';
import { SceneChips } from './scene-chips';

const SCENES: GuideScene[] = [
  {
    slug: 'leather',
    display_name: 'Leather & kink',
    description: '',
    coverage: 'full',
    venue_count: 0,
    organization_count: 0,
    place_count: 0,
  },
];

function render(slugs: string[], scenes?: GuideScene[]): HTMLElement {
  TestBed.configureTestingModule({ providers: [provideRouter([])] });
  const fixture = TestBed.createComponent(SceneChips);
  fixture.componentRef.setInput('slugs', slugs);
  if (scenes) {
    fixture.componentRef.setInput('scenes', scenes);
  }
  fixture.detectChanges();
  return fixture.nativeElement as HTMLElement;
}

function chips(element: HTMLElement) {
  return Array.from(element.querySelectorAll('a.scene-chips__chip')).map((anchor) => ({
    label: anchor.textContent?.trim(),
    href: anchor.getAttribute('href'),
  }));
}

describe('SceneChips', () => {
  it("links each tag to its scene's landing, by its display name", () => {
    expect(chips(render(['leather', 'bear'], SCENES))).toEqual([
      { label: 'Leather & kink', href: '/guide/scenes/leather' },
      // Not in the list it was given: the slug stands in for the name.
      { label: 'bear', href: '/guide/scenes/bear' },
    ]);
  });

  it('renders nothing when there are no tags', () => {
    expect(render([]).children.length).toBe(0);
  });
});
