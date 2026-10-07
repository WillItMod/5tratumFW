import { CanActivateFn, Router } from '@angular/router';
import { inject } from '@angular/core';
import { Observable, map, catchError, of, take, timeout } from 'rxjs';
import { LiveDataService } from '../services/live-data.service';

export const ApModeGuard: CanActivateFn = (): Observable<boolean> => {
  const liveDataService = inject(LiveDataService);
  const router = inject(Router);

  return liveDataService.info$.pipe(
    take(1),
    // An unreachable miner must not prevent the interface shell from opening.
    timeout(3000),
    map(info => {
      if (info.apEnabled) {
        router.navigate(['/ap']);
        return false;
      }
      return true;
    }),
    catchError(() => of(true))
  );
};
