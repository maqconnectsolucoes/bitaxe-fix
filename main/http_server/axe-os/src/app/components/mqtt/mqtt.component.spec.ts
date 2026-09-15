import { ComponentFixture, TestBed } from '@angular/core/testing';
import { FormsModule, ReactiveFormsModule } from '@angular/forms';
import { provideHttpClient } from '@angular/common/http';
import { provideToastr } from 'ngx-toastr';
import { of } from 'rxjs';

import { MqttComponent } from './mqtt.component';
import { CheckboxComponent } from 'src/app/components/checkbox/checkbox.component';
import { TooltipDirective } from 'src/app/directives/tooltip.directive';
import { TooltipTextIconComponent } from 'src/app/components/tooltip-text-icon/tooltip-text-icon.component';
import { TooltipIconComponent } from 'src/app/components/tooltip-icon/tooltip-icon.component';
import { LiveDataService } from 'src/app/services/live-data.service';
import { SystemApiService } from 'src/app/services/system.service';

describe('MqttComponent', () => {
  let component: MqttComponent;
  let fixture: ComponentFixture<MqttComponent>;

  const info = {
    mqttEnabled: 1 as const,
    mqttHost: 'mqtt.exemplo.com',
    mqttPort: 8883,
    mqttUser: 'bitaxe',
    mqttPass: '*****',
    mqttCaCert: '',
    mqttInterval: 30,
    mqttDeviceId: 'bitaxe-3cdc755aa5fc'
  };

  beforeEach(() => {
    TestBed.configureTestingModule({
      declarations: [MqttComponent, TooltipTextIconComponent, TooltipIconComponent],
      imports: [ReactiveFormsModule, FormsModule, CheckboxComponent, TooltipDirective],
      providers: [
        provideHttpClient(),
        provideToastr(),
        { provide: LiveDataService, useValue: { info$: of(info) } }
      ]
    });

    fixture = TestBed.createComponent(MqttComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('loads the stored settings into the form', () => {
    expect(component.form.get('mqttEnabled')?.value).toBeTrue();
    expect(component.form.get('mqttHost')?.value).toBe('mqtt.exemplo.com');
    expect(component.form.get('mqttPort')?.value).toBe(8883);
    expect(component.form.get('mqttInterval')?.value).toBe(30);
  });

  it('sends only the fields the user edited', () => {
    const systemService = TestBed.inject(SystemApiService);
    const update = spyOn(systemService, 'updateSystem').and.returnValue(of({}));

    const host = component.form.get('mqttHost')!;
    host.setValue('broker.exemplo.com');
    host.markAsDirty();
    component.updateSystem();

    expect(update).toHaveBeenCalledTimes(1);
    expect(update.calls.mostRecent().args[1]).toEqual({ mqttHost: 'broker.exemplo.com' });
  });

  it('never sends the password placeholder as a new password', () => {
    // system_api_json.c returns '*****' whenever a password is stored. Sending
    // that back as a real value is the defect the pool passwords had.
    const systemService = TestBed.inject(SystemApiService);
    const update = spyOn(systemService, 'updateSystem').and.returnValue(of({}));

    const user = component.form.get('mqttUser')!;
    user.setValue('outro');
    user.markAsDirty();
    component.updateSystem();

    expect(update.calls.mostRecent().args[1].mqttPass).toBeUndefined();
  });

  it('drops the password even when the field was touched but left as the placeholder', () => {
    const systemService = TestBed.inject(SystemApiService);
    const update = spyOn(systemService, 'updateSystem').and.returnValue(of({}));

    const pass = component.form.get('mqttPass')!;
    pass.markAsDirty();
    component.updateSystem();

    expect(update.calls.mostRecent().args[1].mqttPass).toBeUndefined();
  });

  it('sends mqttEnabled as numeric 0/1, not a JSON boolean', () => {
    // The API boolean convention in CLAUDE.md: numbers on the wire.
    const systemService = TestBed.inject(SystemApiService);
    const update = spyOn(systemService, 'updateSystem').and.returnValue(of({}));

    const enabled = component.form.get('mqttEnabled')!;
    enabled.setValue(false);
    enabled.markAsDirty();
    component.updateSystem();

    expect(update.calls.mostRecent().args[1].mqttEnabled).toBe(0);
  });

  it('blocks the save when the hostname is invalid', () => {
    const systemService = TestBed.inject(SystemApiService);
    const update = spyOn(systemService, 'updateSystem').and.returnValue(of({}));

    const host = component.form.get('mqttHost')!;
    host.setValue('mqtts://broker.exemplo.com:8883');
    host.markAsDirty();

    expect(component.form.invalid).toBeTrue();
    component.updateSystem();

    expect(update).not.toHaveBeenCalled();
  });

  it('requires a hostname only while MQTT is enabled', () => {
    const host = component.form.get('mqttHost')!;
    host.setValue('');
    host.markAsDirty();
    expect(component.form.invalid).toBeTrue();

    component.form.get('mqttEnabled')!.setValue(false);

    expect(component.form.valid).toBeTrue();
  });

  it('pre-fills the device id the firmware resolved, and leaves it editable', () => {
    // system_api_json.c always answers with a value: the stored one, or the
    // MAC-derived default. The screen shows it filled in either way.
    const id = component.form.get('mqttDeviceId')!;
    expect(id.value).toBe('bitaxe-3cdc755aa5fc');
    expect(id.enabled).toBeTrue();
  });

  it('builds the topic from the device id and follows edits to it', () => {
    expect(component.topic).toBe('bitaxe/bitaxe-3cdc755aa5fc/telemetry');

    component.form.get('mqttDeviceId')!.setValue('galpao-02');

    expect(component.topic).toBe('bitaxe/galpao-02/telemetry');
  });

  it('sends the edited device id', () => {
    const systemService = TestBed.inject(SystemApiService);
    const update = spyOn(systemService, 'updateSystem').and.returnValue(of({}));

    const id = component.form.get('mqttDeviceId')!;
    id.setValue('galpao-02');
    id.markAsDirty();
    component.updateSystem();

    expect(update.calls.mostRecent().args[1]).toEqual({ mqttDeviceId: 'galpao-02' });
  });

  it('rejects a device id carrying MQTT topic separators or wildcards', () => {
    // '/', '+' and '#' would split the topic or turn it into a subscription
    // pattern, so the identifier must never contain them.
    const id = component.form.get('mqttDeviceId')!;
    for (const bad of ['galpao/02', 'galpao+02', 'galpao#02', 'galpao 02', '']) {
      id.setValue(bad);
      expect(id.invalid).withContext(`deveria recusar '${bad}'`).toBeTrue();
    }

    id.setValue('galpao-02');
    expect(id.valid).toBeTrue();
  });

  it('stops toggling the hostname requirement after destroy', () => {
    component.form.get('mqttHost')!.setValue('');
    fixture.destroy();

    component.form.get('mqttEnabled')!.setValue(false);

    // With the subscription leaked, destroying the component would still let it
    // relax the validator on a form nobody is showing any more.
    expect(component.form.invalid).toBeTrue();
  });
});
