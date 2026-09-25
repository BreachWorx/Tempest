import { Component } from '@angular/core';
import { ClientMonitorComponent } from './components/client-monitor/client-monitor.component';

@Component({
  selector: 'app-root',
  standalone: true,
  imports: [ClientMonitorComponent],
  template: `<app-client-monitor></app-client-monitor>`
})
export class AppComponent {}
