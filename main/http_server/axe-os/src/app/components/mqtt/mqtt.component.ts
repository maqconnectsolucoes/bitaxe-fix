import { HttpErrorResponse } from '@angular/common/http';
import { Component, Input, OnDestroy, OnInit } from '@angular/core';
import { FormBuilder, FormGroup, Validators } from '@angular/forms';
import { ToastrService } from 'ngx-toastr';
import { Subject, first, takeUntil } from 'rxjs';
import { LoadingService } from 'src/app/services/loading.service';
import { SystemApiService } from 'src/app/services/system.service';
import { LiveDataService } from 'src/app/services/live-data.service';
import { loadOrFail } from 'src/app/operators/load-or-fail';
import { hostnameValidator } from 'src/app/validators/hostname.validator';
import { pemCertificateValidator } from 'src/app/validators/pem-certificate.validator';

/**
 * Same literal system_api_json.c returns in place of a stored password, and the
 * one http_server.c reads as "keep what is already in NVS".
 */
const PASSWORD_PLACEHOLDER = '*****';

/**
 * A '/' would split the topic into extra levels and '+'/'#' would turn it into a
 * subscription pattern, so neither can appear in the identifier.
 */
const DEVICE_ID_PATTERN = /^[A-Za-z0-9._:-]+$/;

@Component({
    selector: 'app-mqtt',
    templateUrl: './mqtt.component.html',
    standalone: false
})
export class MqttComponent implements OnInit, OnDestroy {
  public form!: FormGroup;
  public savedChanges: boolean = false;
  public showPassword: boolean = false;

  private destroy$ = new Subject<void>();

  @Input() uri = '';

  constructor(
    private fb: FormBuilder,
    private systemService: SystemApiService,
    private liveDataService: LiveDataService,
    private toastr: ToastrService,
    private loadingService: LoadingService
  ) { }

  ngOnInit(): void {
    this.liveDataService.info$
      .pipe(first(), loadOrFail(this.toastr), this.loadingService.lockUIUntilComplete(), takeUntil(this.destroy$))
      .subscribe(info => {
        this.form = this.fb.group({
          mqttEnabled: [info.mqttEnabled === 1],
          mqttHost: [info.mqttHost ?? '', [hostnameValidator, Validators.maxLength(128)]],
          mqttPort: [info.mqttPort ?? 8883, [Validators.required, Validators.min(1), Validators.max(65535)]],
          mqttUser: [info.mqttUser ?? '', [Validators.maxLength(64)]],
          mqttPass: [info.mqttPass ?? '', [Validators.maxLength(128)]],
          mqttCaCert: [info.mqttCaCert ?? '', [pemCertificateValidator(), Validators.maxLength(3000)]],
          mqttInterval: [info.mqttInterval ?? 30, [Validators.required, Validators.min(5), Validators.max(3600)]],
          // The firmware always answers with a value here - the stored one, or
          // the MAC-derived default - so the field arrives filled in and stays
          // editable.
          mqttDeviceId: [info.mqttDeviceId ?? '',
            [Validators.required, Validators.pattern(DEVICE_ID_PATTERN), Validators.maxLength(64)]]
        });

        // A broker address is only meaningful while publishing is on, so the
        // requirement follows the toggle - otherwise turning MQTT off would be
        // blocked by a field that no longer matters.
        this.applyHostRequirement(this.form.get('mqttEnabled')?.value);

        this.form.get('mqttEnabled')?.valueChanges
          .pipe(takeUntil(this.destroy$))
          .subscribe((enabled: boolean) => this.applyHostRequirement(enabled));
      });
  }

  ngOnDestroy(): void {
    this.destroy$.next();
    this.destroy$.complete();
  }

  private applyHostRequirement(enabled: boolean): void {
    const host = this.form.get('mqttHost');
    if (!host) return;

    host.setValidators(enabled
      ? [Validators.required, hostnameValidator, Validators.maxLength(128)]
      : [hostnameValidator, Validators.maxLength(128)]);
    host.updateValueAndValidity({ emitEvent: false });
  }

  public updateSystem(): void {
    if (this.form.invalid) {
      this.toastr.error('Please fix the highlighted fields before saving.');
      return;
    }

    // Only the fields the user actually edited are sent: the PATCH handler in
    // http_server.c writes exactly the keys it receives and leaves the rest
    // alone, so an untouched field never needs to travel.
    const raw = this.form.getRawValue();
    const form: Record<string, any> = {};

    for (const key of Object.keys(this.form.controls)) {
      if (!this.form.get(key)?.dirty) continue;

      // An untouched password still reads as the placeholder. Sending it would
      // overwrite the stored one with five asterisks - the defect the pool
      // passwords had before network.edit.component's pattern was copied there.
      if (key === 'mqttPass' && raw[key] === PASSWORD_PLACEHOLDER) continue;

      // Boolean-like fields go on the wire as numbers, per the API convention.
      form[key] = key === 'mqttEnabled' ? (raw[key] ? 1 : 0) : raw[key];
    }

    const restartAlreadyPending = this.savedChanges;

    this.systemService.updateSystem(this.uri, form)
      .pipe(this.loadingService.lockUIUntilComplete())
      .subscribe({
        next: () => {
          this.toastr.warning('You must restart this device after saving for changes to take effect.');
          this.toastr.success(this.uri ? `Saved MQTT settings for ${this.uri}` : 'Saved MQTT settings');
          this.savedChanges = true;
          this.form.markAsPristine();
        },
        error: (err: HttpErrorResponse) => {
          const target = this.uri ? ` for ${this.uri}` : '';
          this.toastr.error(`Could not save MQTT settings${target}. ${err.message}`);
          this.savedChanges = restartAlreadyPending;
        }
      });
  }

  public restart(): void {
    this.systemService.restart(this.uri)
      .pipe(this.loadingService.lockUIUntilComplete())
      .subscribe({
        next: () => {
          this.toastr.success(this.uri ? `Device at ${this.uri} restarted` : 'Device restarted');
          this.savedChanges = false;
        },
        error: (err: HttpErrorResponse) => {
          const target = this.uri ? ` at ${this.uri}` : '';
          this.toastr.error(`Failed to restart device${target}. ${err.message}`);
        }
      });
  }

  public onCertFileSelected(event: Event): void {
    const fileInput = event.target as HTMLInputElement;
    if (!fileInput.files || fileInput.files.length === 0) return;

    const reader = new FileReader();

    reader.onload = () => {
      const cert = this.form.get('mqttCaCert');
      cert?.setValue(reader.result as string);
      cert?.markAsDirty();
      fileInput.value = '';
    };

    reader.onerror = () => {
      this.toastr.error('Failed to read certificate file');
      fileInput.value = '';
    };

    reader.readAsText(fileInput.files[0]);
  }

  /** The topic this miner publishes to, kept visible so the edit is not blind. */
  public get topic(): string {
    return `bitaxe/${this.form?.get('mqttDeviceId')?.value ?? ''}/telemetry`;
  }

  public get isEnabled(): boolean {
    return this.form?.get('mqttEnabled')?.value === true;
  }
}
