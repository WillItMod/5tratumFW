import { TestBed, fakeAsync, tick } from '@angular/core/testing';
import { ActivatedRouteSnapshot, Router, RouterStateSnapshot } from '@angular/router';
import { NEVER, Observable, of, throwError } from 'rxjs';
import { ApModeGuard } from './ap-mode.guard';
import { LiveDataService } from '../services/live-data.service';
import { SystemInfo } from '../generated/models';

describe('AP mode startup guard', () => {
  function run(info$: Observable<SystemInfo>): { result: Observable<boolean>; navigate: jasmine.Spy } {
    const navigate = jasmine.createSpy('navigate').and.returnValue(Promise.resolve(true));
    TestBed.configureTestingModule({ providers: [{ provide: LiveDataService, useValue: { info$ } }, { provide: Router, useValue: { navigate } }] });
    return { result: TestBed.runInInjectionContext(() => ApModeGuard({} as ActivatedRouteSnapshot, {} as RouterStateSnapshot)) as Observable<boolean>, navigate };
  }
  it('opens the shell after a bounded wait when no telemetry arrives', fakeAsync(() => {
    const guard = run(NEVER);
    let allowed: boolean | undefined;
    guard.result.subscribe(result => allowed = result);
    tick(2999);
    expect(allowed).toBeUndefined();
    tick(1);
    expect(allowed).toBeTrue();
    expect(guard.navigate).not.toHaveBeenCalled();
  }));
  it('continues to redirect a confirmed AP device', () => {
    const guard = run(of({ apEnabled: 1 } as SystemInfo));
    let allowed: boolean | undefined;
    guard.result.subscribe(result => allowed = result);
    expect(allowed).toBeFalse();
    expect(guard.navigate).toHaveBeenCalledWith(['/ap']);
  });
  it('opens the normal shell after a real fetch error', () => {
    const guard = run(throwError(() => new Error('Unavailable')));
    let allowed: boolean | undefined;
    guard.result.subscribe(result => allowed = result);
    expect(allowed).toBeTrue();
    expect(guard.navigate).not.toHaveBeenCalled();
  });
});
