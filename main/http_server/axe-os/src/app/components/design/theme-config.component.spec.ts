import { ComponentFixture, TestBed } from '@angular/core/testing';
import { FormsModule } from '@angular/forms';
import { provideHttpClient } from '@angular/common/http';
import { HttpTestingController, provideHttpClientTesting } from '@angular/common/http/testing';
import { ButtonModule } from 'primeng/button';
import { ThemeConfigComponent } from './theme-config.component';
import { ThemeService } from '../../services/theme.service';
import { LayoutService } from '../../layout/service/app.layout.service';
import { environment } from '../../../environments/environment';

describe('Appearance saving', () => {
  let fixture: ComponentFixture<ThemeConfigComponent>;
  let http: HttpTestingController;
  let service: ThemeService;
  let previousProduction: boolean;
  let previousStyle: string;
  const stored = { colorScheme: 'dark', primaryColor: '#F80421' };
  const applyButton = () => fixture.nativeElement.querySelector('.appearance-actions button') as HTMLButtonElement;
  const choose = (name: string) => {
    const button = Array.from(fixture.nativeElement.querySelectorAll('.appearance-swatch'))
      .find((element: any) => element.textContent.includes(name)) as HTMLButtonElement;
    button.click(); fixture.detectChanges();
  };
  const saveResponse = (expected: {colorScheme: string; primaryColor: string}) => {
    const post = http.expectOne(request => request.method === 'POST' && request.url === '/api/theme');
    expect(post.request.body).toEqual(expected);
    post.flush({status: 'ok'});
    const readback = http.expectOne(request => request.method === 'GET' && request.url === '/api/theme');
    readback.flush(expected); fixture.detectChanges();
  };

  beforeEach(() => {
    previousProduction = environment.production; environment.production = true;
    previousStyle = document.documentElement.style.cssText;
    TestBed.configureTestingModule({
      declarations: [ThemeConfigComponent], imports: [FormsModule, ButtonModule],
      providers: [provideHttpClient(), provideHttpClientTesting(), ThemeService, LayoutService],
    });
    service = TestBed.inject(ThemeService); http = TestBed.inject(HttpTestingController);
    TestBed.inject(LayoutService);
    fixture = TestBed.createComponent(ThemeConfigComponent); fixture.detectChanges();
    http.expectOne('/api/theme').flush(stored); fixture.detectChanges();
  });
  afterEach(() => {
    fixture.destroy(); http.verify(); environment.production = previousProduction;
    document.documentElement.style.cssText = previousStyle;
  });

  it('loads the saved primaryColor and disables Apply until there is a change', () => {
    expect(fixture.componentInstance.currentColor).toBe('#f80421');
    expect(applyButton().disabled).toBeTrue();
    expect(document.documentElement.style.getPropertyValue('--primary-color')).toBe('#f80421');
    http.expectNone(request => request.method === 'POST');
  });

  it('saves Orange only on Apply, using the firmware contract, and confirms by reading it back', () => {
    choose('Orange'); expect(applyButton().disabled).toBeFalse();
    expect(fixture.nativeElement.textContent).toContain('Changes are ready to apply');
    http.expectNone(request => request.method === 'POST');
    expect(document.documentElement.style.getPropertyValue('--primary-color')).toBe('#f80421');
    applyButton().click(); fixture.detectChanges();
    expect(applyButton().disabled).toBeTrue();
    saveResponse({colorScheme: 'dark', primaryColor: '#f7931a'});
    expect(document.documentElement.style.getPropertyValue('--primary-color')).toBe('#f7931a');
    expect(fixture.nativeElement.textContent).toContain('saved and confirmed');
    expect(applyButton().disabled).toBeTrue();
  });

  it('keeps the saved colour through Light and Dark changes and a fresh settings load', () => {
    choose('Orange'); applyButton().click();
    saveResponse({colorScheme: 'dark', primaryColor: '#f7931a'});
    fixture.componentInstance.selectedScheme = 'light'; fixture.componentInstance.selectionChanged(); fixture.detectChanges();
    applyButton().click(); saveResponse({colorScheme: 'light', primaryColor: '#f7931a'});
    expect(fixture.componentInstance.currentColor).toBe('#f7931a');
    fixture.componentInstance.selectedScheme = 'dark'; fixture.componentInstance.selectionChanged(); fixture.detectChanges();
    applyButton().click(); saveResponse({colorScheme: 'dark', primaryColor: '#f7931a'});
    service.reload(); http.expectOne('/api/theme').flush({colorScheme: 'dark', primaryColor: '#F7931A'}); fixture.detectChanges();
    expect(document.documentElement.style.getPropertyValue('--primary-color')).toBe('#f7931a');
    expect(fixture.componentInstance.currentColor).toBe('#f7931a');
    expect(applyButton().disabled).toBeTrue();
  });

  it('keeps the draft and reports a failed save without applying or claiming success', () => {
    choose('Orange'); applyButton().click();
    http.expectOne('/api/theme').flush('failed', {status: 500, statusText: 'Failed'}); fixture.detectChanges();
    expect(fixture.nativeElement.querySelector('[role="alert"]').textContent).toContain('Could not confirm');
    expect(fixture.componentInstance.currentColor).toBe('#f7931a');
    expect(document.documentElement.style.getPropertyValue('--primary-color')).toBe('#f80421');
    expect(applyButton().disabled).toBeFalse();
    applyButton().click(); saveResponse({colorScheme: 'dark', primaryColor: '#f7931a'});
  });

  it('does not report success if the firmware acknowledges a save but did not persist the colour', () => {
    choose('Orange'); applyButton().click();
    http.expectOne('/api/theme').flush({status: 'ok'});
    http.expectOne('/api/theme').flush(stored); fixture.detectChanges();
    expect(fixture.componentInstance.hasChanges).toBeTrue();
    expect(fixture.nativeElement.textContent).toContain('Could not confirm');
    expect(fixture.nativeElement.textContent).not.toContain('saved and confirmed');
  });

  it('shows a failed load and allows retry without writing any settings', () => {
    service.reload(); http.expectOne('/api/theme').flush('failed', {status: 503, statusText: 'Unavailable'}); fixture.detectChanges();
    expect(fixture.nativeElement.textContent).toContain('Saved appearance could not be loaded');
    expect(applyButton().disabled).toBeTrue();
    fixture.nativeElement.querySelector('.appearance-error button').click();
    http.expectOne('/api/theme').flush({colorScheme: 'dark', primaryColor: '#2196f3'}); fixture.detectChanges();
    expect(fixture.componentInstance.currentColor).toBe('#2196f3');
    http.expectNone(request => request.method === 'POST');
  });

  it('preserves the colour when a caller saves only a scheme', () => {
    service.saveThemeSettings({colorScheme: 'light'}).subscribe();
    saveResponse({colorScheme: 'light', primaryColor: '#f80421'});
    expect(fixture.componentInstance.currentColor).toBe('#f80421');
  });
});
