import { ComponentFixture, TestBed } from '@angular/core/testing';

import { NetworkEditComponent } from './network.edit.component';
import { provideHttpClient } from '@angular/common/http';
import { provideToastr } from 'ngx-toastr';
import { DialogService } from 'src/app/services/dialog.service';
import { DialogService as PrimeDialogService } from 'primeng/dynamicdialog';
import { FormBuilder } from '@angular/forms';
import { of } from 'rxjs';

describe('NetworkEditComponent', () => {
  let component: NetworkEditComponent;
  let fixture: ComponentFixture<NetworkEditComponent>;

  beforeEach(() => {
    TestBed.configureTestingModule({
      declarations: [NetworkEditComponent],
      providers: [provideHttpClient(), provideToastr(), DialogService, PrimeDialogService]
    });
    fixture = TestBed.createComponent(NetworkEditComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});

describe('Native hostname update response', () => {
  function editor(url:string) {
    const api={getInfo:jasmine.createSpy('getInfo').and.returnValue(of({hostname:'miner-new',ssid:'existing-wifi'})),
      updateSystem:jasmine.createSpy('updateSystem').and.returnValue(of({status:'success',redirect:{url,delay:2000}}))};
    const component=new NetworkEditComponent(new FormBuilder(),api as any,{info$:of({hostname:'miner-new',ssid:'existing-wifi'})} as any,
      {success:()=>{},warning:()=>{},error:()=>{}} as any,{lockUIUntilComplete:()=>((stream:any)=>stream)} as any,{} as any,{} as any);
    component.uri='http://selected-miner.local';component.ngOnInit();return {component,api};
  }
  it('reads the explicitly selected miner and offers its confirmed hostname without following the redirect automatically', () => {
    const {component,api}=editor('http://miner-new.local');component.updateSystem();
    expect(api.getInfo).toHaveBeenCalledWith('http://selected-miner.local');expect(component.savedHostnameUrl).toBe('http://miner-new.local/');
    expect(api.updateSystem.calls.mostRecent().args[0]).toBe('http://selected-miner.local');expect(api.updateSystem.calls.mostRecent().args[1].wifiPass).toBeUndefined();
  });
  it('does not offer an unrelated or credential-bearing redirect received with a settings acknowledgment', () => {
    for(const url of ['https://example.com','http://user:secret@miner-new.local','javascript:alert(1)']) {
      const {component}=editor(url);component.updateSystem();expect(component.savedHostnameUrl).toBeNull();
    }
  });
});
