export interface ClientDevice {
  id: number;
  uuid: string;
  nickname: string;
  computer_name: string;
  os_platform: string | null;
  architecture: string | null;
  status: 'online' | 'offline' | 'idle';
  last_seen_at: string;
  created_at: string;
  updated_at: string;
}
