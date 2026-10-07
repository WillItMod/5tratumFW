import { NgModule } from '@angular/core';
import { RouterModule } from '@angular/router';
import { NbCardModule, NbTooltipModule, NbCheckboxModule, NbSelectModule, NbIconModule, NbButtonModule, NbBadgeModule } from '@nebular/theme';
import { SettingsComponent } from './settings.component';
import { NbThemeModule, NbLayoutModule } from '@nebular/theme';
import { CommonModule } from '@angular/common';
import { AlertModule } from '../alert/alert.module';
import { EditModule } from '../edit/edit.module';
import { NbSpinnerModule } from '@nebular/theme';
import { NbProgressBarModule } from '@nebular/theme';
import { I18nModule } from '../../@i18n/i18n.module';
import { FormsModule, ReactiveFormsModule } from '@angular/forms';

@NgModule({
  declarations: [SettingsComponent],
  imports: [
    CommonModule,
    RouterModule,
    NbCardModule,
    NbLayoutModule,
    NbThemeModule,
    NbIconModule,
    NbButtonModule,
    EditModule,
    AlertModule,
    NbSpinnerModule,
    NbProgressBarModule,
    NbBadgeModule,
    I18nModule,
    NbSelectModule,
    NbCheckboxModule,
    FormsModule,
    ReactiveFormsModule,
    NbTooltipModule,
  ]
})
export class SettingsModule { }
