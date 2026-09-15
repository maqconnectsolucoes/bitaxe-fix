import { ComponentFixture, TestBed } from '@angular/core/testing';

import { EditComponent } from './edit.component';
import { provideHttpClient } from '@angular/common/http';
import { provideToastr } from 'ngx-toastr';
import { provideRouter } from '@angular/router';
import { HttpClient } from '@angular/common/http';
import { of } from 'rxjs';
import { SystemApiService } from 'src/app/services/system.service';

describe('EditComponent', () => {
  let component: EditComponent;
  let fixture: ComponentFixture<EditComponent>;

  beforeEach(() => {
    TestBed.configureTestingModule({
      imports: [EditComponent],
      providers: [provideHttpClient(), provideToastr(), provideRouter([])]
    });
    fixture = TestBed.createComponent(EditComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });

  it('keeps custom displayTimeout/statsFrequency steps per device instance', () => {
    const httpClient = TestBed.inject(HttpClient);
    const asic = { ASICModel: 'BM1370', defaultFrequency: 500, frequencyOptions: [400, 500], defaultVoltage: 1150, voltageOptions: [1100, 1200] };
    spyOn(httpClient, 'get').and.callFake(((url: string) => {
      if (url.includes('/api/system/asic')) return of(asic);
      if (url.startsWith('http://dev1')) return of({ displayTimeout: 7, statsFrequency: 3 });
      return of({ displayTimeout: 5, statsFrequency: 5 });
    }) as any);

    // dev1 uses values outside the predefined steps -> it grows its own step list
    const dev1 = TestBed.createComponent(EditComponent);
    dev1.componentInstance.uri = 'http://dev1';
    dev1.detectChanges();

    // dev2 uses predefined values -> its slider range must stay the default
    const dev2 = TestBed.createComponent(EditComponent);
    dev2.componentInstance.uri = 'http://dev2';
    dev2.detectChanges();

    expect(dev2.componentInstance.displayTimeoutMaxSteps).toBe(10);
    expect(dev2.componentInstance.statsFrequencyMaxSteps).toBe(11);
    expect(dev1.componentInstance.displayTimeoutMaxSteps).toBeGreaterThan(dev2.componentInstance.displayTimeoutMaxSteps);
    expect(dev1.componentInstance.statsFrequencyMaxSteps).toBeGreaterThan(dev2.componentInstance.statsFrequencyMaxSteps);
  });

  describe('with a device whose ASIC presets are known', () => {
    const asic = { ASICModel: 'BM1370', defaultFrequency: 500, frequencyOptions: [400, 500], defaultVoltage: 1150, voltageOptions: [1100, 1200] };
    let dev: ComponentFixture<EditComponent>;
    let update: jasmine.Spy;

    beforeEach(() => {
      const httpClient = TestBed.inject(HttpClient);
      spyOn(httpClient, 'get').and.callFake(((url: string) =>
        of(url.includes('/api/system/asic') ? asic : { frequency: 500, coreVoltage: 1150, temptarget: 60, minFanSpeed: 25, displayTimeout: 5, statsFrequency: 5 })
      ) as any);
      update = spyOn(TestBed.inject(SystemApiService), 'updateSystem').and.returnValue(of({}));
      dev = TestBed.createComponent(EditComponent);
      dev.componentInstance.uri = 'http://dev1';
      dev.detectChanges();
    });

    it('only accepts preset frequencies while overclock is locked', () => {
      const frequency = dev.componentInstance.form.get('frequency')!;
      frequency.setValue(450);
      expect(frequency.valid).toBeFalse();
      frequency.setValue(500);
      expect(frequency.valid).toBeTrue();
    });

    it('accepts any positive frequency once overclock is unlocked (the firmware enforces its envelope)', () => {
      dev.componentInstance.toggleOverclockMode(true);
      const frequency = dev.componentInstance.form.get('frequency')!;
      frequency.setValue(450);
      expect(frequency.valid).toBeTrue();
    });

    it('sends only the fields the user changed', () => {
      const temptarget = dev.componentInstance.form.get('temptarget')!;
      temptarget.setValue(65);
      temptarget.markAsDirty();
      dev.componentInstance.updateSystem();
      expect(update).toHaveBeenCalledTimes(1);
      expect(update.calls.mostRecent().args[1]).toEqual({ temptarget: 65 });
    });

    it('sends overheat_mode when clearing the overheat flag', () => {
      dev.componentInstance.disableOverheatMode();
      expect(update.calls.mostRecent().args[1]).toEqual({ overheat_mode: 0 });
    });

    it('does not demand a restart for a minimum fan speed change', () => {
      dev.componentInstance.form.get('minfanspeed')!.markAsDirty();
      expect(dev.componentInstance.isRestartRequired).toBeFalse();
    });
  });
});
