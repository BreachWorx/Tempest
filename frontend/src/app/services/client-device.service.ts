import { Injectable, inject } from '@angular/core';
import { HttpClient } from '@angular/common/http';
import { Observable } from 'rxjs';
import { ClientDevice } from '../models/client-device.model';

@Injectable({
  providedIn: 'root'
})
export class ClientDeviceService {
  private http = inject(HttpClient);
  private apiUrl = 'http://127.0.0.1:8000/api/clients'; // Update port/host as needed

  getClients(): Observable<ClientDevice[]> {
    return this.http.get<ClientDevice[]>(this.apiUrl);
  }
}
