import { TestBed } from '@angular/core/testing';
import { FreshnessStatus } from '../../../core/types/events.types';
import { pacificWallClockToUs } from '../../../core/utils/events-date.utils';
import { FreshnessFooter } from './freshness-footer';

function render(status: FreshnessStatus, lastVerifiedAtUs: number | null, closedLabel?: string) {
  const fixture = TestBed.createComponent(FreshnessFooter);
  fixture.componentRef.setInput('status', status);
  fixture.componentRef.setInput('lastVerifiedAtUs', lastVerifiedAtUs);
  if (closedLabel) {
    fixture.componentRef.setInput('closedLabel', closedLabel);
  }
  fixture.detectChanges();
  const element = fixture.nativeElement as HTMLElement;
  return {
    marker: element.querySelector('.freshness-footer__marker')?.textContent?.trim() ?? '',
    text: element.textContent?.replace(/\s+/g, ' ').trim() ?? '',
  };
}

describe('FreshnessFooter', () => {
  // New Year's Eve, 8 PM Seattle time: already January in UTC.
  const NEW_YEARS_EVE = pacificWallClockToUs(2026, 12, 31, 20);

  it('dates the last check in Seattle time, and shows no marker when open', () => {
    expect(render('open', NEW_YEARS_EVE)).toEqual({ marker: '', text: 'Verified December 2026' });
    expect(render('unverified', null)).toEqual({ marker: '', text: 'Not yet verified' });
  });

  it('marks a listing that is not simply open', () => {
    expect(render('closed', NEW_YEARS_EVE).marker).toBe('Closed');
    expect(render('closed', NEW_YEARS_EVE, 'Ended').marker).toBe('Ended');
    expect(render('moved', null).marker).toBe('Moved');
    expect(render('seasonal', null).text).toBe('Seasonal Not yet verified');
  });
});
