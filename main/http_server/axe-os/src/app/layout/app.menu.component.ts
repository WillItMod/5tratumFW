import { Component, ElementRef, ViewChild } from '@angular/core';
@Component({ selector: 'app-menu', templateUrl: './app.menu.component.html' })
export class AppMenuComponent {
  @ViewChild('moreMenu') moreMenu?: ElementRef<HTMLDetailsElement>;
  closeMore(): void { if (this.moreMenu) this.moreMenu.nativeElement.open = false; }
}
