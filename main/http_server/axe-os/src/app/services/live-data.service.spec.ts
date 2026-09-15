import { discardPeriodicTasks, fakeAsync, tick } from '@angular/core/testing';
import { of, timer, map } from 'rxjs';

import { LiveDataService } from './live-data.service';
import { SystemApiService } from './system.service';
import { environment } from 'src/environments/environment';

describe('LiveDataService', () => {
  let systemService: jasmine.SpyObj<SystemApiService>;
  let mockBefore: boolean;

  beforeEach(() => {
    mockBefore = environment.mock;
    environment.mock = true; // keeps connect() from opening a real WebSocket
    systemService = jasmine.createSpyObj<SystemApiService>('SystemApiService', ['getInfo']);
  });

  afterEach(() => {
    environment.mock = mockBefore;
  });

  it('emits the initial system info', () => {
    systemService.getInfo.and.returnValue(of({ version: 'v1', uptimeSeconds: 1 } as any));
    const service = new LiveDataService(systemService);
    let seen: any;
    service.info$.subscribe(info => (seen = info));
    expect(seen.version).toBe('v1');
  });

  it('does not start a new fallback poll while the previous one is still in flight', fakeAsync(() => {
    // Without a WebSocket the service polls /api/system/info every 5 s (visible tab);
    // the device may take longer than that to answer. switchMap cancelled the
    // in-flight request on every tick, so a slow device was never read at all.
    const interval = document.visibilityState === 'visible' ? 5000 : 60000;
    const slow = timer(interval * 2 + 500).pipe(map(() => ({ version: 'slow', uptimeSeconds: 2 } as any)));
    systemService.getInfo.and.returnValues(of({ version: 'v1', uptimeSeconds: 1 } as any), slow, slow, slow);

    const service = new LiveDataService(systemService);
    const versions: string[] = [];
    const sub = service.info$.subscribe(info => versions.push(info.version as string));

    tick(interval);       // poll #1 starts
    tick(interval);       // tick #2 while #1 is still pending
    expect(systemService.getInfo).toHaveBeenCalledTimes(2);

    tick(interval + 500); // poll #1 finally answers
    expect(versions).toContain('slow');

    sub.unsubscribe();
    discardPeriodicTasks();
  }));
});
