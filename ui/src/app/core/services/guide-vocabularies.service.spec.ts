import { TestBed } from '@angular/core/testing';
import { firstValueFrom, throwError } from 'rxjs';
import { vi } from 'vitest';
import { GUIDE_ACCESS } from '../guide-access/guide-access';
import { provideGuideAccess } from '../guide-access/guide-access.providers';
import { GuideVocabulariesService, guideVocabularyLabel } from './guide-vocabularies.service';

function configure() {
  TestBed.configureTestingModule({
    providers: [provideGuideAccess(true), GuideVocabulariesService],
  });
}

describe('GuideVocabulariesService', () => {
  it('reads the scenes and neighborhoods once, however many pages ask', async () => {
    configure();
    const access = TestBed.inject(GUIDE_ACCESS);
    const getScenes = vi.spyOn(access, 'getScenes');
    const service = TestBed.inject(GuideVocabulariesService);

    const first = await firstValueFrom(service.vocabularies$);
    const second = await firstValueFrom(service.vocabularies$);

    expect(second).toBe(first);
    expect(getScenes).toHaveBeenCalledTimes(1);
    expect(guideVocabularyLabel(first.scenes, 'leather')).toBe('Leather & kink');
    expect(guideVocabularyLabel(first.neighborhoods, 'capitol-hill')).toBe('Capitol Hill');
  });

  it('keeps going with an empty list when one fails, showing slugs instead', async () => {
    configure();
    vi.spyOn(TestBed.inject(GUIDE_ACCESS), 'getScenes').mockReturnValue(
      throwError(() => new Error('offline')),
    );

    const vocabularies = await firstValueFrom(
      TestBed.inject(GuideVocabulariesService).vocabularies$,
    );

    expect(vocabularies.scenes).toEqual([]);
    expect(vocabularies.neighborhoods.length).toBeGreaterThan(0);
    expect(guideVocabularyLabel(vocabularies.scenes, 'leather')).toBe('leather');
  });
});
