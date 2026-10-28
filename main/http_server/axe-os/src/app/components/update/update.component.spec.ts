import { ComponentFixture, TestBed } from '@angular/core/testing';

import { UpdateComponent } from './update.component';
import { ModalComponent } from '../modal/modal.component';
import { FileUploadModule } from 'primeng/fileupload';
import { CheckboxModule } from 'primeng/checkbox';
import { provideHttpClient } from '@angular/common/http';
import { provideToastr } from 'ngx-toastr';
import { BehaviorSubject, of, throwError } from 'rxjs';
import { HttpErrorResponse, HttpResponse } from '@angular/common/http';
import { SystemInfo } from 'src/app/generated/models';

describe('UpdateComponent', () => {
  let component: UpdateComponent;
  let fixture: ComponentFixture<UpdateComponent>;

  beforeEach(() => {
    TestBed.configureTestingModule({
      declarations: [UpdateComponent, ModalComponent],
      imports: [FileUploadModule, CheckboxModule],
      providers: [provideHttpClient(), provideToastr()]
    });
    fixture = TestBed.createComponent(UpdateComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});

describe('Gamma update identity and authorization', () => {
  let component: UpdateComponent;
  let api:any, live:any, toast:any;
  let readings:BehaviorSubject<SystemInfo>;
  beforeEach(() => {
    api={performOTAUpdate:jasmine.createSpy('performOTAUpdate').and.returnValue(of(new HttpResponse({status:200,body:'ok'}))),performWWWOTAUpdate:jasmine.createSpy('performWWWOTAUpdate')};
    toast=jasmine.createSpyObj('toast',['success','error']);
    readings=new BehaviorSubject({ASICModel:'BM1370',boardVersion:'602',version:'5tratumFW-test'} as SystemInfo);
    live={info$:readings,lastUpdateAt:Date.now()};
    component=new UpdateComponent(api,live,toast,{} as any,{getReleases:()=>of([])} as any,{} as any);
    component.firmwareUpload={clear:()=>{}} as any;component.websiteUpload={clear:()=>{}} as any;
  });
  afterEach(() => component.ngOnDestroy());
  it('blocks both upload paths when the discovered device is another model or telemetry is stale', () => {
    readings.next({ASICModel:'BM1370',boardVersion:'2.2',version:'5tratumFW-oct-test'} as SystemInfo);
    component.otaUpdate({files:[new File(['test'],'esp-miner.bin')]} as any);component.otaWWWUpdate({files:[new File(['test'],'www.bin')]} as any);
    expect(api.performOTAUpdate).not.toHaveBeenCalled();expect(api.performWWWOTAUpdate).not.toHaveBeenCalled();
    readings.next({ASICModel:'BM1370',boardVersion:'601',version:'5tratumFW-test'} as SystemInfo);live.lastUpdateAt=Date.now()-20000;
    expect(component.canUpdate).toBeFalse();
  });
  it('reports an upload acknowledgment separately from a confirmed miner restart', () => {
    component.otaUpdate({files:[new File(['test'],'esp-miner.bin')]} as any);
    expect(api.performOTAUpdate).toHaveBeenCalledTimes(1);expect(component.updateStatus).toBe('success');
    expect(component.updateMessage).toContain('Wait for the miner to restart');expect(toast.success).not.toHaveBeenCalled();
  });
  it('retains an authorization failure without retrying or reporting success', () => {
    api.performOTAUpdate.and.returnValue(throwError(()=>new HttpErrorResponse({status:401,error:{message:'Unauthorized'}})));
    component.otaUpdate({files:[new File(['test'],'esp-miner.bin')]} as any);
    expect(component.updateStatus).toBe('error');expect(component.updateMessage).toBe('Unauthorized');expect(api.performOTAUpdate).toHaveBeenCalledTimes(1);expect(toast.success).not.toHaveBeenCalled();
  });
});
