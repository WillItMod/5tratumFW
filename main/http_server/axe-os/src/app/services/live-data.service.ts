import { Injectable } from '@angular/core';
import { BehaviorSubject, Observable, Subject, EMPTY, timer, merge, fromEvent } from 'rxjs';
import { catchError, retry, share, tap, switchMap, exhaustMap, startWith, scan, shareReplay, map, timeout, bufferTime, filter, distinctUntilChanged } from 'rxjs/operators';
import { webSocket, WebSocketSubject } from 'rxjs/webSocket';
import { SystemInfo as ISystemInfo } from 'src/app/generated/models';
import { SystemApiService } from './system.service';

@Injectable({
  providedIn: 'root'
})
export class LiveDataService {
  private socket$: WebSocketSubject<any> | null = null;
  private updates$ = new Subject<Partial<ISystemInfo>>();
  
  // Shared info stream for the whole app
  public readonly info$: Observable<ISystemInfo>;
  public lastUpdateAt = 0;
  
  // Connection status for the UI
  private connectedSubject = new BehaviorSubject<boolean>(false);
  public connected$ = this.connectedSubject.asObservable();

  constructor(
    private systemService: SystemApiService
  ) {
    // Visibility stream for polling adjustments
    const visibility$ = fromEvent(document, 'visibilitychange').pipe(
      map(() => document.visibilityState),
      startWith(document.visibilityState),
      distinctUntilChanged(),
      shareReplay(1)
    );

    // Periodic polling fallback (adjust frequency based on visibility)
    const fallbackPolling$ = visibility$.pipe(
      switchMap(state => {
        const interval = state === 'visible' ? 5000 : 60000; // 5s when visible, 60s when hidden
        return timer(interval, interval).pipe(
          exhaustMap(() => {
            // Only poll if not connected OR if backgrounded (to keep data fresh)
            if (this.connectedSubject.value && state === 'visible') return EMPTY;
            // A temporary read failure must not stop future polling attempts.
            return this.systemService.getInfo().pipe(catchError(() => EMPTY));
          })
        );
      }),
      catchError(() => EMPTY)
    );

    const updates$ = merge(
      this.connect().pipe(switchMap(() => EMPTY), catchError(() => EMPTY)),
      this.updates$.pipe(
        // Buffer updates to handle bursts when tab is resumed
        bufferTime(500),
        filter(msgs => msgs.length > 0),
        map(msgs => msgs.reduce((acc, curr) => ({ ...acc, ...curr }), {} as Partial<ISystemInfo>))
      ),
      fallbackPolling$
    );

    const initialInfo$ = this.systemService.getInfo().pipe(
      catchError(err => {
        console.error('Initial info fetch failed', err);
        return EMPTY;
      })
    );

    this.info$ = merge(initialInfo$, updates$).pipe(
      // Reject empty/malformed responses before merging with cached readings.
      // Otherwise the old version field would make an invalid response look fresh.
      filter(update => this.isValidUpdate(update)),
      tap(() => this.lastUpdateAt = Date.now()),
      scan((acc: ISystemInfo, curr: Partial<ISystemInfo>) => ({ ...acc, ...curr } as ISystemInfo), {} as ISystemInfo),
      // Ensure we have at least once received a message with a recognizable field before emitting
      filter(info => !!info.version || !!info.uptimeSeconds),
      shareReplay(1)
    );
  }

  private isValidUpdate(update: Partial<ISystemInfo>): boolean {
    if (!update || typeof update !== 'object' || Array.isArray(update)) return false;
    const numericKeys: (keyof ISystemInfo)[] = [
      'uptimeSeconds', 'hashRate', 'power', 'voltage', 'current', 'temp', 'temp2',
      'coreVoltage', 'coreVoltageActual', 'responseTime', 'frequency', 'fanrpm',
    ];
    let hasTelemetry = false;
    for (const key of numericKeys) {
      if (!(key in update)) continue;
      const value = update[key];
      if (typeof value !== 'number' || !Number.isFinite(value)) return false;
      hasTelemetry = true;
    }
    return hasTelemetry || typeof update.miningPaused === 'boolean';
  }

  private connect(): Observable<any> {
    if (this.socket$ || !window.location.host) {
      return EMPTY;
    }

    const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
    const host = window.location.host;
    const url = `${protocol}//${host}/api/ws/live`;

    this.socket$ = webSocket({
      url,
      openObserver: {
        next: () => {
          console.log('Live WebSocket connected');
          this.connectedSubject.next(true);
        }
      },
      closeObserver: {
        next: () => {
          console.log('Live WebSocket disconnected');
          this.connectedSubject.next(false);
          this.socket$ = null;
        }
      }
    });

    return this.socket$.pipe(
      timeout(5000),
      tap(msg => {
        if (msg.event === 'update' && msg.data) {
          this.updates$.next(msg.data);
        }
      }),
      retry({ delay: 5000 }),
      share()
    );
  }
}
