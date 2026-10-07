import { ComponentFixture, TestBed } from '@angular/core/testing';

import { EditComponent } from './edit.component';
import { provideHttpClient } from '@angular/common/http';
import { provideToastr } from 'ngx-toastr';
import { provideRouter } from '@angular/router';

describe('EditComponent', () => {
  let component: EditComponent;
  let fixture: ComponentFixture<EditComponent>;

  beforeEach(() => {
    TestBed.configureTestingModule({
      declarations: [EditComponent],
      providers: [provideHttpClient(), provideToastr(), provideRouter([])]
    });
    fixture = TestBed.createComponent(EditComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});

// Exercise initialization independently of the hardware form template.
import { fakeAsync, tick } from '@angular/core/testing';
import { FormBuilder } from '@angular/forms';
import { ActivatedRoute } from '@angular/router';
import { NEVER, of, ReplaySubject, throwError } from 'rxjs';
import { ToastrService } from 'ngx-toastr';
import { LiveDataService } from 'src/app/services/live-data.service';
import { LoadingService } from 'src/app/services/loading.service';
import { SystemApiService } from 'src/app/services/system.service';
import { OperatingSettingsExportService } from 'src/app/services/operating-settings-export.service';
import { SystemInfo, SystemAsic } from 'src/app/generated/models';

describe('Hardware settings startup recovery', () => {
  const info = {
    frequency: 525, coreVoltage: 1150, display: 'SSD1306 (128x32)', rotation: 0,
    invertscreen: 0, displayTimeout: -1, autofanspeed: 1, minFanSpeed: 30,
    manualFanSpeed: 50, temptarget: 60, overheat_mode: 0, statsFrequency: 60,
  } as SystemInfo;
  const asic: SystemAsic = { ASICModel: 'BM1370', asicCount: 1, deviceModel: 'Gamma', swarmColor: 'purple', defaultFrequency: 525, frequencyOptions: [500, 525], defaultVoltage: 1150, voltageOptions: [1100, 1150] };
  function setup() {
    const readings = new ReplaySubject<SystemInfo>(1);
    const api = jasmine.createSpyObj<SystemApiService>('SystemApiService', ['getInfo', 'getAsicSettings', 'updateSystem']);
    const loading = new LoadingService();
    const component = new EditComponent(new FormBuilder(), api, { info$: readings.asObservable() } as LiveDataService,
      jasmine.createSpyObj<ToastrService>('ToastrService', ['success', 'warning', 'error']), loading,
      { queryParams: of({}) } as ActivatedRoute, {} as OperatingSettingsExportService);
    return { component, readings, api, loading };
  }

  it('recovers from an ASIC read error without blocking the shell or overwriting subsequent edits', fakeAsync(() => {
    const { component, readings, api, loading } = setup();
    api.getAsicSettings.and.returnValues(throwError(() => new Error('Offline')), of(asic));
    component.ngOnInit();
    tick(0);
    expect(component.form).toBeUndefined();
    expect(component.initializationError).toContain('unavailable');
    expect(loading.loading$.value).toBeFalse();
    readings.next(info);
    tick(5000);
    expect(component.form.get('frequency')?.value).toBe(525);
    expect(component.initializationError).toBeNull();
    component.form.get('frequency')!.setValue(540);
    component.form.markAsDirty();
    readings.next({ ...info, frequency: 500 });
    tick(10000);
    expect(api.getAsicSettings).toHaveBeenCalledTimes(2);
    expect(component.form.get('frequency')?.value).toBe(540);
    expect(component.form.dirty).toBeTrue();
    expect(api.updateSystem).not.toHaveBeenCalled();
    component.ngOnDestroy();
  }));

  it('bounds a silent initialization read and initializes only from a later real response', fakeAsync(() => {
    const { component, readings, api, loading } = setup();
    api.getAsicSettings.and.returnValues(NEVER, of(asic));
    component.ngOnInit();
    tick(4500);
    expect(component.form).toBeUndefined();
    expect(component.initializationError).toContain('unavailable');
    expect(loading.loading$.value).toBeFalse();
    readings.next(info);
    tick(500);
    expect(component.form.get('coreVoltage')?.value).toBe(1150);
    expect(component.initializationError).toBeNull();
    expect(api.updateSystem).not.toHaveBeenCalled();
    component.ngOnDestroy();
  }));
});
