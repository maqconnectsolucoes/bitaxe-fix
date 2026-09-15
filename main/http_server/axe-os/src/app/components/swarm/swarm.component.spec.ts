import { ComponentFixture, TestBed, discardPeriodicTasks, fakeAsync, tick } from '@angular/core/testing';
import { FormsModule, ReactiveFormsModule } from '@angular/forms';
import { HttpClient, HttpErrorResponse, provideHttpClient } from '@angular/common/http';
import { ToastrService, provideToastr } from 'ngx-toastr';
import { LocalStorageService } from 'src/app/local-storage.service';
import { defer, finalize, map, of, throwError, timer } from 'rxjs';

import { SwarmComponent } from './swarm.component';
import { ModalComponent } from '../modal/modal.component';
import { TooltipTextIconComponent } from 'src/app/components/tooltip-text-icon/tooltip-text-icon.component';
import { DropdownComponent } from 'src/app/components/dropdown/dropdown.component';
import { SliderComponent } from 'src/app/components/slider/slider.component';
import { TooltipDirective } from 'src/app/directives/tooltip.directive';

import { HashSuffixPipe } from 'src/app/pipes/hash-suffix.pipe';
import { DiffSuffixPipe } from 'src/app/pipes/diff-suffix.pipe';
import { DateAgoPipe } from 'src/app/pipes/date-ago.pipe';
import { AddressPipe } from 'src/app/pipes/address.pipe';
import { SatsPipe } from 'src/app/pipes/sats.pipe';

describe('SwarmComponent', () => {
  let component: SwarmComponent;
  let fixture: ComponentFixture<SwarmComponent>;
  let httpClient: HttpClient;

  beforeEach(() => {
    TestBed.configureTestingModule({
      declarations: [
        SwarmComponent,
        ModalComponent,
        TooltipTextIconComponent
      ],
      imports: [
        ReactiveFormsModule,
        FormsModule,
        TooltipDirective,
        DropdownComponent,
        SliderComponent,
        HashSuffixPipe,
        DiffSuffixPipe,
        DateAgoPipe,
        AddressPipe,
        SatsPipe
      ],
      providers: [
        provideHttpClient(),
        provideToastr()
      ]
    });
    
    httpClient = TestBed.inject(HttpClient);
  });

  function create(getFake: (url: string) => any) {
    spyOn(httpClient, 'get').and.callFake(getFake as any);
    fixture = TestBed.createComponent(SwarmComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  }

  const syncFake = (url: string) => {
    if (url.includes('/api/system/info')) {
      return of({ ipv4: '192.168.1.1', version: 'v2.1.2' });
    }
    return of({});
  };

  it('should create', () => {
    create(syncFake);
    expect(component).toBeTruthy();
  });

  it('limits the network scan to 16 devices in flight (2 requests each)', fakeAsync(() => {
    // The device serving AxeOS has max_open_sockets=20 with LRU purge: a /24
    // scan with 128 concurrent devices (256 sockets) evicts the live WebSocket.
    let inFlight = 0;
    let maxInFlight = 0;
    create((url: string) => defer(() => {
      // Only count the /24 scan itself, not the two lookups of the serving device
      // (window.location.hostname) whose finalize() runs after their next handler.
      const counted = !url.includes(window.location.hostname);
      if (counted) {
        inFlight++;
        maxInFlight = Math.max(maxInFlight, inFlight);
      }
      return timer(20).pipe(
        map(() => url.includes('/api/system/info') ? { ipv4: '192.168.1.1', version: 'v2.1.2', ASICModel: 'BM1370' } : {}),
        finalize(() => { if (counted) inFlight--; })
      );
    }));

    tick(20);            // own /api/system/info -> initSwarm -> scanNetwork
    tick(20);            // mDNS path: server IP lookup
    tick(20 * 254 + 100); // whole /24 scan
    expect(component.scanning).toBeFalse();
    expect(maxInFlight).toBeLessThanOrEqual(32);
    fixture.destroy();
    discardPeriodicTasks();
  }));

  it('pauses the refresh countdown while the tab is hidden', fakeAsync(() => {
    let state: DocumentVisibilityState = 'visible';
    spyOnProperty(document, 'visibilityState', 'get').and.callFake(() => state);
    create(syncFake);
    component.refreshIntervalTime = 30;

    tick(2000);
    expect(component.refreshIntervalTime).toBe(28);

    state = 'hidden';
    document.dispatchEvent(new Event('visibilitychange'));
    tick(5000);
    expect(component.refreshIntervalTime).toBe(28);

    state = 'visible';
    document.dispatchEvent(new Event('visibilitychange'));
    tick(2000);
    expect(component.refreshIntervalTime).toBe(26);

    fixture.destroy();
    discardPeriodicTasks();
  }));

  it('memoizes filteredSwarm and deviceFamilies until the swarm or the filter changes', () => {
    create(syncFake);
    const dev = (address: string, ASICModel: string, deviceModel: string) =>
      ({ address, connectionAddress: address, ASICModel, deviceModel, asicCount: 1 } as any);
    component.swarm = [dev('gamma-1', 'BM1370', 'Gamma'), dev('gamma-2', 'BM1370', 'Gamma'), dev('ultra-1', 'BM1366', 'Ultra')];
    component.filterText = 'gamma';

    const filtered = component.filteredSwarm;
    const families = component.deviceFamilies;
    expect(filtered.map(d => d.address)).toEqual(['gamma-1', 'gamma-2']);
    expect(families.length).toBe(1);
    // Same references on the next change-detection pass: no per-CD recomputation
    expect(component.filteredSwarm).toBe(filtered);
    expect(component.deviceFamilies).toBe(families);

    component.filterText = 'ultra';
    expect(component.filteredSwarm.map(d => d.address)).toEqual(['ultra-1']);
  });

  it('stops persisting the refresh interval after destroy', () => {
    create(syncFake);
    const setNumber = spyOn(TestBed.inject(LocalStorageService), 'setNumber');
    fixture.destroy();
    component.refreshIntervalControl.setValue(10);
    expect(setNumber).not.toHaveBeenCalled();
  });

  it('reports a failed manual add instead of failing silently', () => {
    create((url: string) => url.includes('bad-host')
      ? throwError(() => new HttpErrorResponse({ status: 500, statusText: 'Internal Server Error' }))
      : syncFake(url));
    const error = spyOn(TestBed.inject(ToastrService), 'error');
    component.form.patchValue({ manualAddAddress: 'bad-host' });
    component.add();
    expect(error).toHaveBeenCalled();
  });

  it('flags a device that found a block (showNewBlock is a JSON boolean, not 0/1)', () => {
    create(syncFake);
    // The firmware emits showNewBlock with cJSON_AddBoolToObject, unlike the
    // 0/1 numeric convention used by overheat_mode / isUsingFallbackStratum.
    const note = component.getDeviceNotification({ frequency: 500, showNewBlock: true });
    expect(note?.msg).toBe('Block found');
  });

  it('does not report a block for a device with showNewBlock false', () => {
    create(syncFake);
    expect(component.getDeviceNotification({ frequency: 500, showNewBlock: false })).toBeUndefined();
  });

  it('should render swarm list details and custom components when devices are present', () => {
    create(syncFake);
    component.swarm = [
      {
        address: 'bitaxe-1.local',
        displayName: 'Bitaxe 1',
        connectionAddress: '192.168.1.100',
        ASICModel: 'BM1366',
        deviceModel: 'Ultra',
        swarmColor: 'purple',
        asicCount: 1,
        hashRate: 500e9,
        sharesAccepted: 100,
        sharesRejected: 1,
        bestDiff: 1000,
        bestSessionDiff: 500,
        power: 15,
        temp: 55,
        version: 'v2.1.2',
        uptimeSeconds: 3600,
        poolDifficulty: 1000
      }
    ];
    fixture.detectChanges();

    const element = fixture.nativeElement;
    
    // Verify that components inside *ngIf are rendered
    expect(element.querySelector('app-slider')).toBeTruthy();
    expect(element.querySelector('app-modal')).toBeTruthy();
  });
});
