import { Routes } from '@angular/router';
import { EventDetailPage } from './event-detail';
import { EventsCalendarPage } from './events-calendar';
import { EventsListPage } from './events-list';
import { SeriesDetailPage } from './series-detail';
import { SeriesIndexPage } from './series-index';

// Mounted at /events, lazy-loaded so the events pages stay out of the
// initial bundle. ':id' must come last: listed earlier it would match
// /events/calendar and /events/series as event ids (the knottyyoga
// route-ordering hazard).
const routes: Routes = [
  { path: '', component: EventsListPage },
  { path: 'calendar', component: EventsCalendarPage },
  { path: 'series', component: SeriesIndexPage },
  { path: 'series/:slug', component: SeriesDetailPage },
  { path: ':id', component: EventDetailPage },
];

export default routes;
