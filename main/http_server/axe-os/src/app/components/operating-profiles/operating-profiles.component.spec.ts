import { of, Subject } from 'rxjs';
import { OperatingProfilesComponent } from './operating-profiles.component';
import { OperatingProfilesService, Profiles } from 'src/app/services/operating-profiles.service';
import { LiveDataService } from 'src/app/services/live-data.service';
import { ComponentFixture, TestBed } from '@angular/core/testing';
import { CommonModule } from '@angular/common';
import { FormsModule } from '@angular/forms';
const data: Profiles = { schemaVersion: 1, tuning: Array.from({ length: 10 }, (_, slot) => ({ slot, configured: false, name: null })), pools: Array.from({ length: 10 }, (_, slot) => ({ slot, configured: slot === 2, name: slot === 2 ? 'MUX' : null, host: 'mux.example.test', port: 3333 })), limits: { frequency: { min: 400, max: 625, step: .25, quantization: 'nearest-pll' }, coreVoltage: { min: 1000, max: 1250, step: 1 } }, current: { frequencyMHz: 625.125, coreVoltageMv: 1173 } };
describe('Miner operating profiles', () => {
 let component: OperatingProfilesComponent; let service: jasmine.SpyObj<OperatingProfilesService>;
 beforeEach(() => { service = jasmine.createSpyObj<OperatingProfilesService>('profiles', ['get', 'save', 'apply']); service.get.and.returnValue(of(structuredClone(data))); service.save.and.returnValue(of({ ok: true, restartRequired: false })); service.apply.and.returnValue(of({ ok: true, restartRequired: false })); component = new OperatingProfilesComponent(service, { info$: new Subject() } as unknown as LiveDataService); });
 afterEach(() => component.ngOnDestroy());
 it('retains the existing fractional point without writing when opening manual mode', () => { component.ngOnInit(); component.manual = true; expect(component.frequencyMHz).toBe(625.125); expect(component.coreVoltageMv).toBe(1173); expect(component.pointValid).toBeTrue(); expect(component.pointChanged).toBeFalse(); expect(service.save).not.toHaveBeenCalled(); expect(service.apply).not.toHaveBeenCalled(); });
 it('reviews without writing and applies the reviewed point only on Apply', () => { component.ngOnInit(); component.frequencyMHz = 525.25; component.coreVoltageMv = 1151; component.reviewManual(); expect(service.apply).not.toHaveBeenCalled(); component.apply(); expect(service.apply).toHaveBeenCalledOnceWith({ type: 'tuning', frequencyMHz: 525.25, coreVoltageMv: 1151 }, ''); });
 it('blocks changed points outside bounds or off the request increment', () => { component.ngOnInit(); for (const frequency of [626, 525.1, NaN]) { component.frequencyMHz = frequency; component.reviewManual(); expect(component.review).toBeUndefined(); } expect(service.apply).not.toHaveBeenCalled(); });
 it('saves a named point on the miner without applying it', () => { component.ngOnInit(); component.names[4] = 'My point'; component.save(4); expect(service.save).toHaveBeenCalledOnceWith({ type: 'tuning', slot: 4, name: 'My point', frequencyMHz: 625.125, coreVoltageMv: 1173 }, ''); expect(service.apply).not.toHaveBeenCalled(); });
 it('uses the reviewed pool target and keeps passwords out of browser Apply', () => { component.type = 'pool'; component.ngOnInit(); component.poolTarget = 'fallback'; component.reviewSlot(component.slots[2]); component.poolTarget = 'primary'; component.apply(); expect(service.apply).toHaveBeenCalledOnceWith({ type: 'pool', slot: 2, poolTarget: 'fallback' }, ''); });
 it('applies a stored pool directly once and blocks empty slots without sending credentials or tuning', () => { component.type = 'pool'; component.ngOnInit(); component.applyPoolSlot(component.slots[0]); expect(service.apply).not.toHaveBeenCalled(); component.poolTarget='primary'; component.applyPoolSlot(component.slots[2]); expect(service.apply).toHaveBeenCalledOnceWith({type:'pool',slot:2,poolTarget:'primary'},''); expect(service.save).not.toHaveBeenCalled(); });
 it('keeps controls busy until the committed snapshot is refreshed', () => { component.ngOnInit(); const next = new Subject<Profiles>(); service.get.and.returnValue(next); component.names[0] = 'Point'; component.save(0); expect(component.busy).toBeTrue(); component.clear(0); expect(service.save).toHaveBeenCalledTimes(1); next.next(structuredClone(data)); next.complete(); expect(component.busy).toBeFalse(); });
});

describe('Operating profile review placement', () => {
 let fixture: ComponentFixture<OperatingProfilesComponent>;
 let component: OperatingProfilesComponent;
 let service: jasmine.SpyObj<OperatingProfilesService>;
 const button = (root: Element, text: string) => Array.from(root.querySelectorAll('button')).find(item => item.textContent?.trim() === text)!;
 const rows = () => Array.from((fixture.nativeElement as HTMLElement).querySelectorAll<HTMLElement>('.profiles-slot'));
 const panels = () => Array.from((fixture.nativeElement as HTMLElement).querySelectorAll<HTMLElement>('.profiles-review'));
 beforeEach(async () => {
   const configured = structuredClone(data);
   configured.tuning[2] = {slot: 2, configured: true, name: 'Morning', frequencyMHz: 525.25, coreVoltageMv: 1151};
   configured.tuning[5] = {slot: 5, configured: true, name: 'Evening', frequencyMHz: 550, coreVoltageMv: 1160};
   service = jasmine.createSpyObj<OperatingProfilesService>('profiles', ['get', 'save', 'apply']);
   service.get.and.returnValue(of(configured));
   service.save.and.returnValue(of({ok: true, restartRequired: false}));
   service.apply.and.returnValue(of({ok: true, restartRequired: false}));
   await TestBed.configureTestingModule({declarations: [OperatingProfilesComponent], imports: [CommonModule, FormsModule],
     providers: [{provide: OperatingProfilesService, useValue: service}, {provide: LiveDataService, useValue: {info$: new Subject()}}]}).compileComponents();
   fixture = TestBed.createComponent(OperatingProfilesComponent);
   component = fixture.componentInstance;
   fixture.detectChanges();
   await fixture.whenStable();
 });
 it('places manual confirmation immediately below its Review and writes only on its Apply', async () => {
   component.manual = true;
   component.frequencyMHz = 525.25;
   component.coreVoltageMv = 1151;
   fixture.detectChanges(); await fixture.whenStable();
   const review = button(fixture.nativeElement, 'Review operating point');
   review.click(); fixture.detectChanges();
   expect(panels().length).toBe(1);
   expect(review.nextElementSibling).toBe(panels()[0]);
   expect(panels()[0].closest('.profiles-slot')).toBeNull();
   expect(service.save).not.toHaveBeenCalled(); expect(service.apply).not.toHaveBeenCalled();
   button(panels()[0], 'Apply').click(); fixture.detectChanges();
   expect(service.apply).toHaveBeenCalledOnceWith({type: 'tuning', frequencyMHz: 525.25, coreVoltageMv: 1151}, '');
   expect(panels().length).toBe(0);
 });
 it('moves the single confirmation to the selected tuning row and applies only that reviewed slot', () => {
   button(rows()[2], 'Review').click(); fixture.detectChanges();
   expect(panels().length).toBe(1); expect(panels()[0].parentElement).toBe(rows()[2]);
   expect(rows()[5].querySelector('.profiles-review')).toBeNull();
   button(rows()[5], 'Review').click(); fixture.detectChanges();
   expect(panels().length).toBe(1); expect(panels()[0].parentElement).toBe(rows()[5]);
   expect(rows()[2].querySelector('.profiles-review')).toBeNull();
   expect(service.save).not.toHaveBeenCalled(); expect(service.apply).not.toHaveBeenCalled();
   button(panels()[0], 'Apply').click(); fixture.detectChanges();
   expect(service.apply).toHaveBeenCalledOnceWith({type: 'tuning', slot: 5}, '');
 });
 it('replaces a slot review with a manual review, and Cancel clears the application intent', async () => {
   button(rows()[2], 'Review').click(); fixture.detectChanges();
   component.manual = true; component.frequencyMHz = 550; component.coreVoltageMv = 1160;
   fixture.detectChanges(); await fixture.whenStable();
   const review = button(fixture.nativeElement, 'Review operating point');
   review.click(); fixture.detectChanges();
   expect(panels().length).toBe(1); expect(review.nextElementSibling).toBe(panels()[0]);
   expect(rows()[2].querySelector('.profiles-review')).toBeNull();
   button(panels()[0], 'Cancel').click(); fixture.detectChanges();
   component.apply();
   expect(panels().length).toBe(0); expect(service.apply).not.toHaveBeenCalled(); expect(service.save).not.toHaveBeenCalled();
 });
 it('removes manual confirmation when an input changes or manual mode closes', async () => {
   component.manual = true; component.frequencyMHz = 550; component.coreVoltageMv = 1160;
   fixture.detectChanges(); await fixture.whenStable();
   button(fixture.nativeElement, 'Review operating point').click(); fixture.detectChanges();
   const input: HTMLInputElement = fixture.nativeElement.querySelector('.profiles-step input');
   input.value = '551'; input.dispatchEvent(new Event('input'));
   await fixture.whenStable(); fixture.detectChanges();
   expect(panels().length).toBe(0); component.apply(); expect(service.apply).not.toHaveBeenCalled();
   button(fixture.nativeElement, 'Review operating point').click(); fixture.detectChanges();
   expect(panels().length).toBe(1);
   const toggle: HTMLInputElement = fixture.nativeElement.querySelector('input[type=checkbox]');
   toggle.checked = false; toggle.dispatchEvent(new Event('change'));
   await fixture.whenStable(); fixture.detectChanges();
   expect(panels().length).toBe(0); expect(component.review).toBeUndefined();
   component.apply(); expect(service.apply).not.toHaveBeenCalled(); expect(service.save).not.toHaveBeenCalled();
 });
});
