import { provideRouter } from '@angular/router';
import { ANSIPipe } from 'src/app/pipes/ansi.pipe';
import { CommonModule } from '@angular/common';
import { SystemApiService } from 'src/app/services/system.service';
import { provideHttpClient } from '@angular/common/http';
import { TooltipDirective } from '../../directives/tooltip.directive';
import { ReactiveFormsModule } from '@angular/forms';
import { provideToastr } from 'ngx-toastr';
import { LogsComponent } from './logs.component';
import { ComponentFixture, TestBed } from '@angular/core/testing';
import { Subject } from 'rxjs';
import { WebsocketService } from 'src/app/services/web-socket.service';

describe('LogsComponent', () => {
  let component: LogsComponent;
  let fixture: ComponentFixture<LogsComponent>;
  let ws$: Subject<string>;

  beforeEach(async () => {
    ws$ = new Subject<string>();
    await TestBed.configureTestingModule({
      declarations: [LogsComponent],
      imports: [
        CommonModule,
        ReactiveFormsModule,
        TooltipDirective,
        ANSIPipe
      ],
      providers: [
        provideRouter([]),
        provideToastr(),
        provideHttpClient(),
        SystemApiService,
        { provide: WebsocketService, useValue: { ws$ } }
      ]
    })
    .compileComponents();
    
    fixture = TestBed.createComponent(LogsComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });

  it('scrolls to the bottom only when a new log line arrived', () => {
    const container: HTMLElement = fixture.nativeElement.querySelector('#logs');
    const scrollTo = spyOn(container, 'scrollTo');

    fixture.detectChanges();
    fixture.detectChanges();
    expect(scrollTo).not.toHaveBeenCalled();

    ws$.next('I (1) main: hello\n');
    fixture.detectChanges();
    expect(scrollTo).toHaveBeenCalledTimes(1);

    fixture.detectChanges();
    expect(scrollTo).toHaveBeenCalledTimes(1);
  });
});
