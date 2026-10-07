import { Component } from '@angular/core';

@Component({
  selector: 'ngx-one-column-layout',
  styleUrls: ['./one-column.layout.scss'],
  template: `
    <nb-layout class="fw-command-shell">
      <nb-layout-header fixed>
        <ngx-header [navigationOpen]="navigationOpen" (navigationToggle)="toggleNavigation()"></ngx-header>
      </nb-layout-header>
      <nb-layout-column>
        <div class="fw-command-navigation" [class.fw-navigation-open]="navigationOpen">
          <ng-content select="[fw-navigation]"></ng-content>
        </div>
        <main class="fw-main" id="fw-main"><ng-content select="router-outlet"></ng-content></main>
      </nb-layout-column>
      <nb-layout-footer>
        <span class="fw-footer">5tratumFW <span>Device interface</span></span>
      </nb-layout-footer>
    </nb-layout>
  `,
})
export class OneColumnLayoutComponent {
  navigationOpen = false;
  toggleNavigation(): void { this.navigationOpen = !this.navigationOpen; }
  closeNavigation(): void { this.navigationOpen = false; }
}
