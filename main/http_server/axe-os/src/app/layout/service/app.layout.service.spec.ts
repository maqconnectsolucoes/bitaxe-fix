import { TestBed } from '@angular/core/testing';
import { of } from 'rxjs';

import { LayoutService } from './app.layout.service';
import { ThemeService } from 'src/app/services/theme.service';

describe('LayoutService', () => {
  let themeService: jasmine.SpyObj<ThemeService>;

  beforeEach(() => {
    themeService = jasmine.createSpyObj<ThemeService>('ThemeService', ['getThemeSettings', 'saveThemeSettings']);
    themeService.getThemeSettings.and.returnValue(of({ colorScheme: 'dark', primaryColor: '#f80421' } as any));
    themeService.saveThemeSettings.and.returnValue(of({} as any));
    TestBed.configureTestingModule({ providers: [{ provide: ThemeService, useValue: themeService }] });
  });

  it('fetches the theme settings once, not again on every theme change', () => {
    const service = TestBed.inject(LayoutService);
    service.changeTheme();
    service.changeTheme();
    expect(themeService.getThemeSettings).toHaveBeenCalledTimes(1);
    expect(document.documentElement.style.getPropertyValue('--color-primary')).toBe('#f80421');
  });
});
