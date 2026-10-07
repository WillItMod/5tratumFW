import { Component, OnDestroy, OnInit } from '@angular/core';
import { HttpClient } from '@angular/common/http';
import { TranslateService } from '@ngx-translate/core';
import { catchError } from 'rxjs/operators';
import { of, Subscription } from 'rxjs';
import { IIdentifyV2 } from '../models/IIdentifyV2';

interface CommandLink { title: string; icon: string; link: string; home?: boolean; }

@Component({
    selector: 'ngx-pages',
    styleUrls: ['pages.component.scss'],
    templateUrl: './pages.component.html',
  })
  export class PagesComponent implements OnInit, OnDestroy {
    primaryMenu: CommandLink[] = [
        { title: 'Overview', icon: 'activity-outline', link: '/pages/home', home: true },
        { title: 'Miner controls', icon: 'options-2-outline', link: '/pages/settings' },
        { title: 'Scheduler', icon: 'calendar-outline', link: '/pages/scheduler' },
        { title: 'Pool routing', icon: 'shuffle-2-outline', link: '/pages/pool' },
        { title: 'Network', icon: 'wifi-outline', link: '/pages/network' },
        { title: 'Updates', icon: 'download-outline', link: '/pages/update' },
    ];
    moreMenu: CommandLink[] = [];

    private canEnabled = false;
    private langSub?: Subscription;

    constructor(private translateService: TranslateService, private http: HttpClient) {}

    ngOnInit(): void {
        this.buildMenu();
        this.http.get<IIdentifyV2>('/api/v2/identify').pipe(
            catchError(() => of(null))
        ).subscribe(info => {
            this.canEnabled = info?.can?.enabled === true;
            this.buildMenu();
        });

        this.langSub = this.translateService.onLangChange.subscribe(() => {
            this.buildMenu();
        });
    }

    ngOnDestroy(): void {
        this.langSub?.unsubscribe();
    }

    private buildMenu(): void {
        const items: CommandLink[] = [
            { title: 'Security', icon: 'shield-outline', link: '/pages/security' },
            { title: 'Alerts', icon: 'bell-outline', link: '/pages/alert' },
            { title: 'InfluxDB', icon: 'archive-outline', link: '/pages/influxdb' },
            { title: 'System', icon: 'code-outline', link: '/pages/system' },
            { title: 'Network miners', icon: 'grid-outline', link: '/pages/swarm' },
        ];
        if (this.canEnabled) items.push({ title: 'CAN fleet', icon: 'share-outline', link: '/pages/can-fleet' });
        this.moreMenu = items;
    }
}
