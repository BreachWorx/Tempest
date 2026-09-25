import { Component, OnInit, ViewChild, AfterViewInit, inject } from '@angular/core';
import { CommonModule } from '@angular/common';
import { FormsModule } from '@angular/forms';
import { MatTableDataSource, MatTableModule } from '@angular/material/table';
import { MatPaginator, MatPaginatorModule } from '@angular/material/paginator';
import { MatSort, MatSortModule } from '@angular/material/sort';
import { MatInputModule } from '@angular/material/input';
import { MatFormFieldModule } from '@angular/material/form-field';
import { MatButtonModule } from '@angular/material/button';
import { MatIconModule } from '@angular/material/icon';
import { ClientDeviceService } from '../../services/client-device.service';
import { ClientDevice } from '../../models/client-device.model';

export interface ClientData {
  id: string;
  nickname: string;
  ip: string;
  os: 'Windows' | 'macOS' | 'Linux' | 'Android' | 'iOS';
  status: 'Online' | 'Idle' | 'Offline';
  port: number;
}

@Component({
  selector: 'app-client-monitor',
  standalone: true,
  imports: [
    CommonModule,
    FormsModule,
    MatTableModule,
    MatPaginatorModule,
    MatSortModule,
    MatInputModule,
    MatFormFieldModule,
    MatButtonModule,
    MatIconModule
  ],
  templateUrl: './client-monitor.component.html',
  styleUrls: ['./client-monitor.component.css']
})
export class ClientMonitorComponent implements OnInit, AfterViewInit {
  private clientService = inject(ClientDeviceService);
  displayedColumns: string[] = ['action', 'nickname', 'ip', 'os', 'status'];
  dataSource!: MatTableDataSource<ClientData>;

  @ViewChild(MatPaginator) paginator!: MatPaginator;
  @ViewChild(MatSort) sort!: MatSort;

  private prefixes = ['Salty', 'Cyber', 'Neon', 'Cosmic', 'Quantum', 'Glitch', 'Shadow', 'Pixel'];
  private nouns = ['Penguin', 'Ninja', 'Panda', 'Falcon', 'Wanderer', 'Vortex', 'Coder', 'Spectre'];
  private osList: ClientData['os'][] = ['Windows', 'macOS', 'Linux', 'Android', 'iOS'];

  private clients: ClientDevice[] = [];
  private loading = true;

  ngOnInit() {
    const initialData = Array.from({ length: 12 }, () => this.generateRandomClient());
    this.dataSource = new MatTableDataSource(initialData);

    this.clientService.getClients().subscribe({
      next: (data: ClientDevice[]) => {
        this.clients = data;
        console.log(this.clients);
        this.loading = false;
      },
      error: (err: any) => {
        console.error('Failed to fetch clients', err);
        this.loading = false;
      }
    });

  }

  ngAfterViewInit() {
    this.dataSource.paginator = this.paginator;
    this.dataSource.sort = this.sort;
  }

  applyFilter(event: Event) {
    const filterValue = (event.target as HTMLInputElement).value;
    this.dataSource.filter = filterValue.trim().toLowerCase();
  }

  generateRandomClient(): ClientData {
    const prefix = this.prefixes[Math.floor(Math.random() * this.prefixes.length)];
    const noun = this.nouns[Math.floor(Math.random() * this.nouns.length)];
    const tag = Math.floor(1000 + Math.random() * 9000);
    const os = this.osList[Math.floor(Math.random() * this.osList.length)];
    const statuses: ClientData['status'][] = ['Online', 'Online', 'Idle', 'Offline'];
    const status = statuses[Math.floor(Math.random() * statuses.length)];

    return {
      id: Math.random().toString(36).substring(2, 9),
      nickname: `${prefix}${noun}#${tag}`,
      ip: `${Math.floor(Math.random() * 200 + 10)}.${Math.floor(Math.random() * 255)}.${Math.floor(Math.random() * 255)}.${Math.floor(Math.random() * 255)}`,
      os: os,
      status: status,
      port: 3790
    };
  }

  addClient() {
    const newClient = this.generateRandomClient();
    this.dataSource.data = [newClient, ...this.dataSource.data];
  }

  copyIp(ip: string) {
    navigator.clipboard.writeText(ip);
  }
}
