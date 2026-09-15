import { ComponentFixture, TestBed } from '@angular/core/testing';
import { FormsModule, ReactiveFormsModule } from '@angular/forms';
import { provideHttpClient } from '@angular/common/http';
import { provideToastr } from 'ngx-toastr';
import { of } from 'rxjs';

import { PoolComponent } from './pool.component';
import { CheckboxComponent } from 'src/app/components/checkbox/checkbox.component';
import { DropdownComponent } from 'src/app/components/dropdown/dropdown.component';
import { RadioButtonComponent } from 'src/app/components/radio-button/radio-button.component';
import { TooltipDirective } from 'src/app/directives/tooltip.directive';
import { TooltipTextIconComponent } from 'src/app/components/tooltip-text-icon/tooltip-text-icon.component';
import { TooltipIconComponent } from 'src/app/components/tooltip-icon/tooltip-icon.component';
import { LiveDataService } from 'src/app/services/live-data.service';
import { SystemApiService } from 'src/app/services/system.service';

function pool(id: number, url: string) {
  return {
    id,
    stratumProtocol: 'SV1',
    stratumURL: url,
    stratumPort: 3333,
    stratumUser: `user${id}`,
    stratumPassword: '*****',
    stratumSuggestedDifficulty: 0,
    stratumExtranonceSubscribe: false,
    stratumTLS: 0,
    stratumCert: '',
    stratumDecodeCoinbase: true,
    stratumV2ChannelType: 'extended',
    stratumV2AuthorityPubkey: ''
  };
}

describe('PoolComponent', () => {
  let component: PoolComponent;
  let fixture: ComponentFixture<PoolComponent>;

  const info = {
    ASICModel: 'BM1370',
    primaryPoolIndex: 0,
    secondaryPoolIndex: 1,
    pools: [pool(0, 'pool-a.example'), pool(1, 'pool-b.example')]
  };

  beforeEach(() => {
    TestBed.configureTestingModule({
      declarations: [PoolComponent, TooltipTextIconComponent, TooltipIconComponent],
      imports: [
        ReactiveFormsModule,
        FormsModule,
        CheckboxComponent,
        DropdownComponent,
        RadioButtonComponent,
        TooltipDirective
      ],
      providers: [
        provideHttpClient(),
        provideToastr(),
        { provide: LiveDataService, useValue: { info$: of(info) } }
      ]
    });

    fixture = TestBed.createComponent(PoolComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
    expect(component.poolsArray.length).toBe(2);
  });

  it('sends the untouched password placeholder so the firmware keeps the stored password', () => {
    // http_server.c only preserves the stored password when it receives the
    // literal '*****'; an absent field is written as "x".
    const systemService = TestBed.inject(SystemApiService);
    const update = spyOn(systemService, 'updateSystem').and.returnValue(of({}));

    const user = component.poolsArray.at(0).get('stratumUser')!;
    user.setValue('renamed');
    user.markAsDirty();
    component.updateSystem();

    expect(update).toHaveBeenCalledTimes(1);
    const payload = update.calls.mostRecent().args[1];
    expect(payload.pools[0].id).toBe(0);
    expect(payload.pools[0].stratumPassword).toBe('*****');
  });

  it('sends only the pools that were edited (the firmware rewrites every pool it receives)', () => {
    const systemService = TestBed.inject(SystemApiService);
    const update = spyOn(systemService, 'updateSystem').and.returnValue(of({}));

    const user = component.poolsArray.at(1).get('stratumUser')!;
    user.setValue('renamed');
    user.markAsDirty();
    component.updateSystem();

    const payload = update.calls.mostRecent().args[1];
    expect(payload.pools.map((p: any) => p.id)).toEqual([1]);
    expect(payload.primaryPoolIndex).toBeUndefined();
  });

  it('includes a newly added pool in the payload', () => {
    const systemService = TestBed.inject(SystemApiService);
    const update = spyOn(systemService, 'updateSystem').and.returnValue(of({}));

    component.addPool();
    component.updateSystem();

    const payload = update.calls.mostRecent().args[1];
    expect(payload.pools.map((p: any) => p.id)).toEqual([2]);
  });

  it('stops reacting to pool index changes after destroy', () => {
    fixture.destroy();

    // With the valueChanges subscriptions still alive, picking primary == secondary
    // would swap the secondary back to the previous primary (0).
    component.form.get('primaryPoolIndex')?.setValue(1);

    expect(component.form.get('secondaryPoolIndex')?.value).toBe(1);
  });
});
