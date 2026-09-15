import { HttpErrorResponse } from '@angular/common/http';
import { Component, Input, OnInit, OnDestroy, OnChanges, SimpleChanges } from '@angular/core';
import { AbstractControl, FormBuilder, FormGroup, FormControl, ValidationErrors, ValidatorFn, Validators, ReactiveFormsModule } from '@angular/forms';
import { ToastrService } from 'ngx-toastr';
import { forkJoin, startWith, Subject, takeUntil, pairwise, BehaviorSubject, Observable, first } from 'rxjs';
import { LoadingService } from 'src/app/services/loading.service';
import { LiveDataService } from 'src/app/services/live-data.service';
import { SystemApiService } from 'src/app/services/system.service';
import { ActivatedRoute } from '@angular/router';
import { DateAgoPipe } from 'src/app/pipes/date-ago.pipe';
import { DropdownComponent } from '../dropdown/dropdown.component';
import { SelectOption } from '../../models/select-option.model';
import { CommonModule } from '@angular/common';
import { loadOrFail } from 'src/app/operators/load-or-fail';
import { TooltipDirective } from '../../directives/tooltip.directive';
import { CheckboxComponent } from '../checkbox/checkbox.component';
import { SliderComponent } from '../slider/slider.component';

// Predefined slider steps. Each component instance works on its own copy (see
// displayTimeoutSteps / statsFrequencySteps): a device's custom value must not
// leak into the slider range of the next device edited from the swarm.
const DISPLAY_TIMEOUT_STEPS: readonly number[] = Object.freeze([0, 1, 2, 5, 15, 30, 60, 60 * 2, 60 * 4, 60 * 8, -1]);
const STATS_FREQUENCY_STEPS: readonly number[] = Object.freeze([0, 1, 2, 5, 10, 30, 60, 60 * 2, 60 * 6, 60 * 14, 60 * 28, 60 * 60]);

@Component({
    selector: 'app-edit',
    templateUrl: './edit.component.html',
    standalone: true,
    imports: [
        CommonModule,
        ReactiveFormsModule,
        CheckboxComponent,
        DropdownComponent,
        SliderComponent,
        TooltipDirective,
        DateAgoPipe,
    ]
})

export class EditComponent implements OnInit, OnDestroy, OnChanges {
  private formSubject = new BehaviorSubject<FormGroup | null>(null);
  public form$: Observable<FormGroup | null> = this.formSubject.asObservable();

  public form!: FormGroup;

  public savedChanges: boolean = false;
  public settingsUnlocked: boolean = false;

  @Input() uri = '';

  // Store frequency and voltage options from API
  public defaultFrequency: number = 0;
  public frequencyOptions: number[] = [];
  public defaultVoltage: number = 0;
  public voltageOptions: number[] = [];
  public selectFrequencyOptions: SelectOption[] = [];
  public selectVoltageOptions: SelectOption[] = [];

  // Last frequency confirmed stable by autotune (read-only, informational)
  public autotuneFrequency: number = 0;

  private destroy$ = new Subject<void>();

  public displays = ["NONE", "SSD1306 (128x32)", "SSD1309 (128x64)", "SH1107 (64x128)", "SH1107 (128x128)"];
  public rotations = [0, 90, 180, 270];
  public displayTimeoutControl: FormControl;
  public statsFrequencyControl: FormControl;
  public displayTimeoutSteps: number[] = [...DISPLAY_TIMEOUT_STEPS];
  public statsFrequencySteps: number[] = [...STATS_FREQUENCY_STEPS];
  public statsLimit: number = 720;

  constructor(
    private fb: FormBuilder,
    private systemService: SystemApiService,
    private liveDataService: LiveDataService,
    private toastr: ToastrService,
    private loadingService: LoadingService,
    private route: ActivatedRoute,
  ) {
    // Check URL parameter for settings unlock
    this.route.queryParams.pipe(takeUntil(this.destroy$)).subscribe(params => {
      const urlOcParam = params['oc'] !== undefined;
      if (urlOcParam) {
        // If ?oc is in URL, enable overclock and save to NVS
        this.settingsUnlocked = true;
        this.saveOverclockSetting(1);
        console.log(
          '🎉 The ancient seals have been broken!\n' +
          '⚡ Unlimited power flows through your miner...\n' +
          '🔧 You can now set custom frequency and voltage values.\n' +
          '⚠️ Remember: with great power comes great responsibility!'
        );
      } else {
        // If ?oc is not in URL, check NVS setting (will be loaded in ngOnInit)
        console.log('🔒 Here be dragons! Advanced settings are locked for your protection. \n' +
          'Only the bravest miners dare to venture forth... \n' +
          'If you wish to unlock dangerous overclocking powers, add: %c?oc',
          'color: #ff4400; text-decoration: underline; cursor: pointer; font-weight: bold;',
          'to the current URL'
        );
      }
    });

    this.displayTimeoutControl = new FormControl();
    this.displayTimeoutControl.valueChanges.pipe(pairwise(), takeUntil(this.destroy$)).subscribe(([prev, next]) => {
      if (prev === next) {
        return;
      }

      this.form.patchValue({ displayTimeout: this.displayTimeoutSteps[next] });
      this.form.controls['displayTimeout'].markAsDirty();
    });

    this.statsFrequencyControl = new FormControl();
    this.statsFrequencyControl.valueChanges.pipe(pairwise(), takeUntil(this.destroy$)).subscribe(([prev, next]) => {
      if (prev === next) {
        return;
      }

      this.form.patchValue({ statsFrequency: this.statsFrequencySteps[next] });
      this.form.controls['statsFrequency'].markAsDirty();
    });
  }

  // Locked: only the presets the firmware advertises are selectable (min/max
  // alone accepted anything in between, e.g. 1237 mV). Unlocked: the firmware
  // enforces its own per-ASIC envelope, so only require a positive number.
  // An empty options list (device did not answer) must not produce
  // Math.min(...[]) === Infinity, which would lock the Save button.
  private tuningValidators(options: number[]): ValidatorFn[] {
    const validators: ValidatorFn[] = [Validators.required];
    if (this.settingsUnlocked || !options.length) {
      validators.push(Validators.min(1));
    } else {
      validators.push(this.inSetValidator(options));
    }
    return validators;
  }

  private inSetValidator(options: number[]): ValidatorFn {
    return (control: AbstractControl): ValidationErrors | null => {
      if (control.value == null || control.value === '') return null;
      return options.includes(Number(control.value)) ? null : { notAPreset: { options } };
    };
  }

  private refreshTuningValidators() {
    const pairs: [string, number[]][] = [['coreVoltage', this.voltageOptions], ['frequency', this.frequencyOptions]];
    for (const [name, options] of pairs) {
      const control = this.form?.get(name);
      if (!control) continue;
      control.setValidators(this.tuningValidators(options));
      control.updateValueAndValidity();
    }
  }

  private saveOverclockSetting(enabled: number) {
    const deviceUri = this.uri || '';
    this.systemService.updateSystem(deviceUri, { overclockEnabled: enabled })
      .subscribe({
        next: () => {
          console.log(`Overclock setting saved: ${enabled === 1 ? 'enabled' : 'disabled'}`);
        },
        error: (err) => {
          console.error(`Failed to save overclock setting: ${err.message}`);
        }
      });
  }

  ngOnInit(): void {
    this.loadDeviceSettings();
  }

  ngOnChanges(changes: SimpleChanges): void {
    // When URI changes, reload the device settings
    if (changes['uri'] && changes['uri'].currentValue && !changes['uri'].firstChange) {
      this.loadDeviceSettings();
    }
  }

  private loadDeviceSettings(): void {
    const deviceUri = this.uri || '';

    const info$ = deviceUri
      ? this.systemService.getInfo(deviceUri)
      : this.liveDataService.info$.pipe(first());

    // Fetch both system info and ASIC settings in parallel
    forkJoin({
      info: info$,
      asic: this.systemService.getAsicSettings(deviceUri)
    })
    .pipe(
      loadOrFail(this.toastr),
      this.loadingService.lockUIUntilComplete(),
      takeUntil(this.destroy$)
    )
    .subscribe(({ info, asic }) => {
      // Fresh step lists for this device (loadDeviceSettings also runs on uri change)
      this.displayTimeoutSteps = [...DISPLAY_TIMEOUT_STEPS];
      this.statsFrequencySteps = [...STATS_FREQUENCY_STEPS];

      // Store the frequency and voltage options from the API
      this.defaultFrequency = asic.defaultFrequency;
      this.frequencyOptions = asic.frequencyOptions;
      this.defaultVoltage = asic.defaultVoltage;
      this.voltageOptions = asic.voltageOptions;
      this.statsLimit = info.statsLimit || 720;
      this.autotuneFrequency = info.autotuneFrequency || 0;

      // Check if overclock is enabled in NVS
      if (info.overclockEnabled) {
        this.settingsUnlocked = true;
        console.log(
          '🎉 Overclock mode is enabled from NVS settings!\n' +
          '⚡ Custom frequency and voltage values are available.'
        );
      }

        this.form = this.fb.group({
          display: [info.display, [Validators.required]],
          rotation: [info.rotation, [Validators.required]],
          invertscreen: [info.invertscreen == 1],
          displayTimeout: [info.displayTimeout, [
            Validators.required,
            Validators.min(-1),
            Validators.max(this.displayTimeoutMaxValue)
          ]],
          coreVoltage: [info.coreVoltage, this.tuningValidators(this.voltageOptions)],
          frequency: [info.frequency, this.tuningValidators(this.frequencyOptions)],
          autofanspeed: [info.autofanspeed == 1, [Validators.required]],
          autotuneEnabled: [info.autotuneEnabled == 1, [Validators.required]],
          minfanspeed: [info.minFanSpeed, [Validators.required]],
          manualFanSpeed: [info.manualFanSpeed, [Validators.required]],
          temptarget: [info.temptarget, [Validators.required]],
          overheat_mode: [info.overheat_mode, [Validators.required]],
          statsFrequency: [info.statsFrequency, [
            Validators.required,
            Validators.min(0),
            Validators.max(this.statsFrequencyMaxValue)
          ]]
        });

        this.formSubject.next(this.form);

        this.updateSelectOptions();

        this.form.controls['frequency'].valueChanges.pipe(
          takeUntil(this.destroy$)
        ).subscribe(() => this.updateSelectOptions());

        this.form.controls['coreVoltage'].valueChanges.pipe(
          takeUntil(this.destroy$)
        ).subscribe(() => this.updateSelectOptions());

      this.form.controls['autofanspeed'].valueChanges.pipe(
        startWith(this.form.controls['autofanspeed'].value),
        takeUntil(this.destroy$)
      ).subscribe(autofanspeed => {
        if (autofanspeed) {
          this.form.controls['manualFanSpeed'].disable();
          this.form.controls['temptarget'].enable();
        } else {
          this.form.controls['manualFanSpeed'].enable();
          this.form.controls['temptarget'].disable();
        }
      });

      // Add custom value to predefined steps
      if (!this.displayTimeoutSteps.includes(info.displayTimeout)) {
        this.displayTimeoutSteps.push(info.displayTimeout);
        this.displayTimeoutSteps.sort((a, b) => a - b);
        // keep -1 ("never") as the last step
        this.displayTimeoutSteps.push(this.displayTimeoutSteps.shift() as number);
      }

      this.displayTimeoutControl.setValue(
        this.displayTimeoutSteps.findIndex(x => x === info.displayTimeout)
      );

      // Add custom value to predefined steps
      if (!this.statsFrequencySteps.includes(info.statsFrequency)) {
        this.statsFrequencySteps.push(info.statsFrequency);
        this.statsFrequencySteps.sort((a, b) => a - b);
      }

      this.statsFrequencyControl.setValue(
        this.statsFrequencySteps.findIndex(x => x === info.statsFrequency)
      );
    });
  }

  ngOnDestroy(): void {
    this.destroy$.next();
    this.destroy$.complete();
  }

  public updateSystem() {
    // Only the controls the user touched: a full getRawValue() rewrote every
    // NVS key on every save and re-applied frequency/voltage needlessly.
    const raw = this.form.getRawValue();
    const form: Record<string, any> = {};
    for (const [field, control] of Object.entries(this.form.controls)) {
      if (control.dirty) {
        form[field] = raw[field];
      }
    }

    const deviceUri = this.uri || '';
    const restartAlreadyPending = this.savedChanges;
    const restartRequired = this.isRestartRequired;

    this.systemService.updateSystem(deviceUri, form)
      .pipe(this.loadingService.lockUIUntilComplete())
      .subscribe({
        next: () => {
          const successMessage = this.uri ? `Saved settings for ${this.uri}` : 'Saved settings';
          if (restartRequired) {
            this.toastr.warning('You must restart this device after saving for changes to take effect.');
          }
          this.toastr.success(successMessage);
          this.form.markAsPristine();
          this.savedChanges = restartAlreadyPending || restartRequired;
        },
        error: (err: HttpErrorResponse) => {
          const errorMessage = this.uri ? `Could not save settings for ${this.uri}. ${err.message}` : `Could not save settings. ${err.message}`;
          this.toastr.error(errorMessage);
          this.savedChanges = restartAlreadyPending;
        }
      });
  }

  disableOverheatMode() {
    this.form.patchValue({ overheat_mode: 0 });
    // updateSystem() only sends dirty controls
    this.form.controls['overheat_mode'].markAsDirty();
    this.updateSystem();
  }

  toggleOverclockMode(enable: boolean) {
    this.settingsUnlocked = enable;
    this.saveOverclockSetting(enable ? 1 : 0);
    this.refreshTuningValidators();

    if (enable) {
      console.log(
        '🎉 Overclock mode enabled!\n' +
        '⚡ Custom frequency and voltage values are now available.'
      );
    } else {
      console.log('🔒 Overclock mode disabled. Using safe preset values only.');
    }
  }

  public restart() {
    this.systemService.restart(this.uri)
      .pipe(this.loadingService.lockUIUntilComplete())
      .subscribe({
        next: () => {
          const successMessage = this.uri ? `Device at ${this.uri} restarted` : 'Device restarted';
          this.toastr.success(successMessage);
          this.savedChanges = false;
        },
        error: (err: HttpErrorResponse) => {
          const errorMessage = this.uri ? `Failed to restart device at ${this.uri}. ${err.message}` : `Failed to restart device. ${err.message}`;
          this.toastr.error(errorMessage);
        }
      });
  }

  private updateSelectOptions() {
    this.selectFrequencyOptions = this.buildSelectOptions('frequency', this.frequencyOptions, this.defaultFrequency);
    this.selectVoltageOptions = this.buildSelectOptions('coreVoltage', this.voltageOptions, this.defaultVoltage);
  }

  get displayOptions(): SelectOption[] {
    return this.displays.map(display => ({ name: display, value: display }));
  }

  get rotationOptions(): SelectOption[] {
    return this.rotations.map(rotation => ({ name: `${rotation}°`, value: rotation }));
  }

  get displayTimeoutMaxSteps(): number {
    return this.displayTimeoutSteps.length - 1;
  }

  get displayTimeoutMaxValue(): number {
    return this.displayTimeoutSteps[this.displayTimeoutMaxSteps - 1];
  }

  get statsFrequencyMaxSteps(): number {
    return this.statsFrequencySteps.length - 1;
  }

  get statsFrequencyMaxValue(): number {
    return this.statsFrequencySteps[this.statsFrequencyMaxSteps];
  }

  buildSelectOptions(formField: string, apiOptions: number[], defaultValue: number): SelectOption[] {
    if (!apiOptions.length) {
      return [];
    }

    // Convert options from API to select format
    const options = apiOptions.map(option => {
      return {
        name: defaultValue === option ? `${option} (Default)` : `${option}`,
        value: option
      };
    });

    // Get current field value from form
    const currentValue = this.form?.get(formField)?.value;

    // If current field value exists and isn't in the options
    if (currentValue && !options.some(opt => opt.value === currentValue)) {
      options.push({
        name: `${currentValue} (Custom)`,
        value: currentValue
      });
      // Sort options by value
      options.sort((a, b) => a.value - b.value);
    }

    return options;
  }

  get noRestartFields(): string[] {
    return [
      'displayTimeout',
      'coreVoltage',
      'frequency',
      'autofanspeed',
      'manualFanSpeed',
      'temptarget',
      'overheat_mode',
      'statsFrequency',
      'autotuneEnabled',
      // fan_controller_task reads minfanspeed live, no restart needed
      'minfanspeed'
    ];
  }

  get isRestartRequired(): boolean {
    return !! Object.entries(this.form.controls)
      .filter(([field, control]) => control.dirty && !this.noRestartFields.includes(field)).length
  }
}
