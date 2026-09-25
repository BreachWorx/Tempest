import { ComponentFixture, TestBed } from '@angular/core/testing';
import { ClientMonitor } from './client-monitor';

describe('ClientMonitor', () => {
  let component: ClientMonitor;
  let fixture: ComponentFixture<ClientMonitor>;

  beforeEach(async () => {
    await TestBed.configureTestingModule({
      imports: [ClientMonitor],
    }).compileComponents();

    fixture = TestBed.createComponent(ClientMonitor);
    component = fixture.componentInstance;
    await fixture.whenStable();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});
