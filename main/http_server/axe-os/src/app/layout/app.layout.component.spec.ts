import { Subject, of } from 'rxjs';
import { Renderer2 } from '@angular/core';
import { Router } from '@angular/router';
import { AppLayoutComponent } from './app.layout.component';
import { LayoutService } from './service/app.layout.service';
import { SensitiveData } from 'src/app/services/sensitive-data.service';
import { LiveDataService } from 'src/app/services/live-data.service';
import { SystemInfo } from 'src/app/generated/models';

describe('Unavailable device shell recovery', () => {
  it('keeps device data unavailable until a real reading arrives and detects delayed AP mode', () => {
    const readings = new Subject<SystemInfo>();
    const router = { url: '/', events: new Subject(), navigate: jasmine.createSpy('navigate') };
    const layout = { overlayOpen$: new Subject() };
    const component = new AppLayoutComponent(layout as unknown as LayoutService, {} as Renderer2, router as unknown as Router, { hidden: of(false) } as SensitiveData, { info$: readings.asObservable() } as LiveDataService);
    component.ngOnInit();
    expect(component.hasTelemetry).toBeFalse();
    readings.next({ apEnabled: 0 } as SystemInfo);
    expect(component.hasTelemetry).toBeTrue();
    expect(router.navigate).not.toHaveBeenCalled();
    readings.next({ apEnabled: 1 } as SystemInfo);
    expect(router.navigate).toHaveBeenCalledWith(['/ap']);
    router.url = '/ap';
    readings.next({ apEnabled: 1 } as SystemInfo);
    expect(router.navigate).toHaveBeenCalledTimes(1);
    component.ngOnDestroy();
    router.events.complete();
  });
});
