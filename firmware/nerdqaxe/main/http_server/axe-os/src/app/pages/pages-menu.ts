import { NbMenuItem } from '@nebular/theme';
export const MENU_ITEMS: NbMenuItem[] = [
  { title: 'Overview', icon: 'activity-outline', link: '/pages/home', home: true },
  { title: 'Miner controls', icon: 'options-2-outline', link: '/pages/settings' },
  { title: 'Scheduler', icon: 'calendar-outline', link: '/pages/scheduler' },
  { title: 'Pool routing', icon: 'shuffle-2-outline', link: '/pages/pool' },
  { title: 'Network', icon: 'wifi-outline', link: '/pages/network' },
  { title: 'Updates', icon: 'download-outline', link: '/pages/update' },
  { title: 'Network miners', icon: 'grid-outline', link: '/pages/swarm' },
  { title: 'Security', icon: 'shield-outline', link: '/pages/security' },
  { title: 'System', icon: 'code-outline', link: '/pages/system' },
];
