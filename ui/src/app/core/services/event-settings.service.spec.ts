import { TestBed } from '@angular/core/testing';
import { firstValueFrom, throwError } from 'rxjs';
import { vi } from 'vitest';
import { COMMUNITY_ACCESS } from '../community-access/community-access';
import { provideCommunityAccess } from '../community-access/community-access.providers';
import { PastEventSettingsMockStore } from '../mock/past-event-settings-mock-store';
import { EventSettingsService } from './event-settings.service';

function configure() {
  TestBed.configureTestingModule({ providers: [provideCommunityAccess(true)] });
}

describe('EventSettingsService', () => {
  it('reads the settings once, however many pages ask', async () => {
    configure();
    TestBed.inject(PastEventSettingsMockStore).save({
      archived_events_visible_days: 90,
      events_show_past_by_default: true,
    });
    const getEventSettings = vi.spyOn(TestBed.inject(COMMUNITY_ACCESS), 'getEventSettings');
    const service = TestBed.inject(EventSettingsService);

    const first = await firstValueFrom(service.settings$);
    const second = await firstValueFrom(service.settings$);

    expect(second).toBe(first);
    expect(getEventSettings).toHaveBeenCalledTimes(1);
    expect(first).toEqual({ archived_events_visible_days: 90, events_show_past_by_default: true });
  });

  it('falls back to the defaults when they cannot be read', async () => {
    configure();
    vi.spyOn(TestBed.inject(COMMUNITY_ACCESS), 'getEventSettings').mockReturnValue(
      throwError(() => new Error('offline')),
    );

    expect(await firstValueFrom(TestBed.inject(EventSettingsService).settings$)).toEqual({
      archived_events_visible_days: 365,
      events_show_past_by_default: false,
    });
  });
});
